# Design Decisions & Suggestions

---

## Part 2 — Why I wrote it this way (Assuming this is pattern being followed in actual code base that I join, Though I have new ideas to improve, the initial thought is to follow current practices without breaking the flow of current code base, If I have enough time, I would then try to think about improvements in current code base but still follow the concept that current project should not break at any stage). The high level diagram is also added [Ambient Light WOrk flow](https://github.com/minamulhaq/ch/blob/master/part2_new_component/ambient_light_flow.drawio)


### ISR removes itself before giving the semaphore

I copied this pattern from the reference `door_sensor` and kept it deliberately. The idea is simple: if the ISR stays registered while the task is sitting in the 30ms debounce delay, the ALERT pin could bounce and fire a second interrupt before the first one is even processed. By removing the handler as the very first thing in the ISR, that can't happen. The handler only comes back at the very end of the task loop, after `INT_STATUS` is cleared and the callback has finished. It's a one-shot by design.

### Static semaphore instead of dynamic

I used `xSemaphoreCreateBinaryStatic` because I didn't want a heap allocation in the init path. Dynamic allocation means you have to handle the NULL case, and on a memory-constrained system that's a real failure mode. The static version puts the storage in BSS — it's always there, no allocation needed, no cleanup needed if something else in init fails.

### `goto` for error cleanup in `ambientLightInit`

There are six I2C operations in init, each of which can fail, and every failure needs the same cleanup: remove the GPIO ISR handler. Without `goto` you either duplicate that line six times or you nest the calls, which gets unreadable fast. The `init_fail:` label keeps the cleanup in one place. I declared all variables at the top of the function so the jump doesn't skip any initialisations — that would be a C99 violation. ESP-IDF uses this pattern all over its own source, and the project's coding rules explicitly allow it for error handling.

### Register address as first byte in `i2cWrite`

`i2cRead` takes the register address as a separate parameter. `i2cWrite` doesn't — it just takes a raw byte buffer. So the register address has to be the first byte in the buffer you pass in, followed by the data. That's standard I2C framing: you write the register address first, then the value. Nothing unusual here, just following the API contract.

---

## Things I'd do differently on a real project 

The real bugs in RTOS are coming from race conditions, I would design something where I try to remove race conditions as much as possible. An idea is demonstrated here: [Ambient Light, Active Object Patter](https://github.com/minamulhaq/ch/blob/master/decisions.drawio)

### 1. One task owns the I2C bus — active object pattern

Right now, `hw_i2c` protects the bus with a mutex. That means any task calling `ambientLightRead()` or `ambientLightSetThreshold()` can block waiting for the bus, and if a high-priority task ends up waiting behind a low-priority one, you have priority inversion. It works, but it's fragile as the system grows. Even with priority inheritance/cieling, lower priority task must finish first before giving the mutex to higher priority task.

What I'd do instead: give the I2C bus to a single dedicated task. Nothing else ever touches the hardware directly. When another task wants something done, it fills in a command struct and posts it to a queue, then blocks waiting for a direct task notification back.

There are two queues, not one: a high-priority queue for urgent commands like sleep and wake, and a normal queue for everything else like reads and threshold updates, logs etc. The I2C task always checks the high-priority queue first with a zero timeout — non-blocking, instant check. If something's there, it processes it immediately. If the high-priority queue is empty, it falls through and waits on the normal queue with `portMAX_DELAY`. This way an urgent sleep command is never stuck behind a batch of queued reads.

Once the I2C task finishes a command, it writes the result directly into the caller's own variable — the command struct carried a pointer to it — and then fires a direct task notification to wake only that caller. No result queue, no broadcast, no guessing which response belongs to which caller.

```
IDLE — check HIGH queue first (timeout=0, non-blocking), then NORMAL (portMAX_DELAY)
  │
  ├── CMD_ALERT [from ISR via HIGH queue]
  │     vTaskDelay(30ms)         debounce
  │     i2cRead INT_STATUS       clear ALERT pin
  │     i2cRead DATA_LOW+HIGH    get lux
  │     callback(lux)            notify application
  │     gpio_isr_handler_add()   re-arm ISR
  │     → IDLE
  │
  ├── CMD_SLEEP / CMD_WAKE [HIGH queue]
  │     i2cWrite(CONTROL, ...)   → write *pRet → xTaskNotify(caller) → IDLE
  │
  ├── CMD_READ [NORMAL queue]
  │     i2cRead DATA_LOW+HIGH    → write *pResult → xTaskNotify(caller) → IDLE
  │
  └── CMD_SET_THRESH [NORMAL queue]
        i2cWrite(THRESHOLD, ...) → write *pRet    → xTaskNotify(caller) → IDLE
```

The mutex disappears completely. There's no concurrent access to protect against because only one task ever touches the I2C peripheral. Ownership is structural — you can't accidentally bypass it. This is the pattern I've used in production for any shared bus with more than one device on it.

### 2. Split I/O and processing so unit tests don't need mocks

The way `ambientLightRead()` is written now, the I2C call and the byte-combining logic are in the same function. To write a unit test for the byte-combining, you have to mock the I2C layer — set up a `StubWithCallback`, fill in the output buffer, tell CMock to ignore the pointer argument. That's a lot of machinery just to test `(HIGH << 8) | LOW`.

The problem is that mock-based tests are coupled to the implementation, not the behaviour. If I change which register I read from, or split the read into two calls, the test breaks — even if the logic is still correct.

What I'd do instead: split every function that mixes I/O with logic into two:

```c
// I/O wrapper — thin, no logic, tested in integration only
static esp_err_t s_readLuxRaw(uint16_t *pLux)
{
    uint8_t aubData[2] = {0};
    esp_err_t eRet = i2cRead(ALS3000_ADDR, REG_DATA_LOW, aubData, 2);
    if (ESP_OK != eRet) { return eRet; }
    *pLux = s_combineLuxBytes(aubData[0], aubData[1]);
    return ESP_OK;
}

// Pure function — no I/O, unit tested directly
static uint16_t s_combineLuxBytes(uint8_t ubLow, uint8_t ubHigh)
{
    return ((uint16_t)ubHigh << 8) | ubLow;
}
```

Now the unit test is just:

```c
void testLuxCombine(void)
{
    TEST_ASSERT_EQUAL_UINT16(1000, s_combineLuxBytes(0xE8, 0x03));
}
```

No mocks. No CMock setup. No `StubWithCallback`. The I2C wrapper gets tested in integration against real hardware. The logic gets tested in unit tests with plain inputs and outputs. Each type of test does what it's actually good at.

### 3. Task notifications instead of a binary semaphore

The semaphore I used to wake the alert task is actually heavier than it needs to be. A FreeRTOS binary semaphore is a queue of length 1 under the hood — it has a control block, a storage buffer, and an internal lock. That's ~88 bytes on ESP32 just to pass a single "wake up" signal.

FreeRTOS has a lighter mechanism for exactly this case: direct-to-task notifications. Every task already has a notification counter in its TCB, so there's nothing to allocate. The ISR writes directly into that counter:

```c
// ISR — task handle is all you need, no semaphore object
vTaskNotifyGiveFromISR(s_pTaskHandler, &ubHigherPriorityTaskWoken);

// Task — clears the notification on take
ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
```

`s_pSemaphoreHandle` and `s_sSemaphoreBuffer` are gone. The init function is simpler and has one fewer failure point to clean up. FreeRTOS docs put task notifications at around 45% faster than the semaphore equivalent because there's no queue machinery in the path.

The only real constraint is that each task has one notification slot (or 32 indexed slots in FreeRTOS 10.4+), so it only works cleanly when a single thing is waking the task. That's exactly the situation here — only the ALERT ISR ever signals this task — so it's a clean fit.
