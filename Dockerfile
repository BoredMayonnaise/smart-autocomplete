FROM ubuntu:24.04 AS build

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake ca-certificates libasio-dev python3 && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON \
 && cmake --build build --config Release --parallel 2 \
 && ctest --test-dir build --output-on-failure

FROM ubuntu:24.04 AS runtime

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=build /src/build/smartautocomplete /app/smartautocomplete
COPY --from=build /src/static /app/static
COPY --from=build /src/data /app/data

EXPOSE 8080
CMD ["/app/smartautocomplete", "--root", "/app", "--host", "0.0.0.0", "--port", "8080", "--public-host", "zug-ddz.duckdns.org"]