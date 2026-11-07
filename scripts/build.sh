#!/usr/bin/env bash
set -e

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo "==> 1. Gerando Stubs Python com grpcio-tools..."
cd "$PROJECT_ROOT/client_Python"
./venv/bin/python3 -m pip install -r requirements.txt -q
./venv/bin/python3 -m grpc_tools.protoc \
    -I"$PROJECT_ROOT/proto" \
    --python_out=. \
    --grpc_python_out=. \
    "$PROJECT_ROOT/proto/file_processor.proto"

echo "==> 2. Compilando Servidor C++ com CMake..."
mkdir -p "$PROJECT_ROOT/server_cpp/build"
cd "$PROJECT_ROOT/server_cpp/build"
cmake ..
make -j$(nproc)

echo "==> Build concluido com sucesso!"
