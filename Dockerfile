FROM espressif/idf:v5.0.9

RUN apt-get update && apt-get install -y \
    libbsd-dev \
    g++ \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /challenge

RUN echo "source /opt/esp/idf/export.sh" >> /root/.bashrc

CMD ["/bin/bash"]