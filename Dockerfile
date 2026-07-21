FROM drogonframework/drogon:latest

WORKDIR /app

COPY . .

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --parallel

EXPOSE 8080

CMD ["./build/smart_university_advisor"]
