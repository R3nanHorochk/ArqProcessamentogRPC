import os
import sys
import grpc
from pathlib import Path

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import file_processor_pb2
import file_processor_pb2_grpc

CHUNK_SIZE = 64 * 1024

def file_chunk_generator(file_path, params_dict):
    p = Path(file_path)
    req = file_processor_pb2.FileRequest(file_name=p.name, **params_dict)
    yield req
    with open(file_path, "rb") as f:
        while True:
            data = f.read(CHUNK_SIZE)
            if not data:
                break
            yield file_processor_pb2.FileRequest(chunk=file_processor_pb2.FileChunk(content=data))

def receive_and_save(response_stream, default_output):
    output_filename = default_output
    saved = False
    with open(output_filename, "wb") as f:
        for resp in response_stream:
            if not resp.success and resp.status_message:
                print(f"[-] Erro do servidor: {resp.status_message}")
                return False
            if resp.file_name:
                output_filename = resp.file_name
            if resp.chunk.content:
                f.write(resp.chunk.content)
                saved = True
    if saved:
        print(f"[+] Sucesso! Arquivo gerado salvo como: {output_filename}")
        return True
    return False

def main():
    channel = grpc.insecure_channel('localhost:50051')
    stub = file_processor_pb2_grpc.FileProcessorServiceStub(channel)

    print("\n=== SERVICO gRPC DE PROCESSAMENTO DE ARQUIVOS ===")
    print("1. Compressao de PDF (Ghostscript)")
    print("2. Conversao de PDF para TXT (Poppler)")
    print("3. Conversao de Formato de Imagem (ImageMagick)")
    print("4. Redimensionamento de Imagem (ImageMagick)")
    print("0. Sair")
    opcao = input("Selecione uma opcao (0-4): ").strip()

    if opcao == "0":
        return

    path = input("Caminho do arquivo de entrada: ").strip()
    if not os.path.exists(path):
        print(f"[-] Arquivo nao encontrado: {path}")
        return

    try:
        if opcao == "1":
            params = {"compress_pdf_params": file_processor_pb2.CompressPDFRequest()}
            stream = stub.CompressPDF(file_chunk_generator(path, params))
            receive_and_save(stream, "compressed_output.pdf")
        elif opcao == "2":
            params = {"convert_to_txt_params": file_processor_pb2.ConvertToTXTRequest()}
            stream = stub.ConvertToTXT(file_chunk_generator(path, params))
            receive_and_save(stream, "output.txt")
        elif opcao == "3":
            fmt = input("Informe o formato de saida (ex: png, jpg, webp): ").strip().lower()
            params = {"convert_image_format_params": file_processor_pb2.ConvertImageFormatRequest(output_format=fmt)}
            stream = stub.ConvertImageFormat(file_chunk_generator(path, params))
            receive_and_save(stream, f"converted_output.{fmt}")
        elif opcao == "4":
            w = int(input("Largura (width) desejada em pixels: "))
            h = int(input("Altura (height) desejada em pixels: "))
            params = {"resize_image_params": file_processor_pb2.ResizeImageRequest(width=w, height=h)}
            stream = stub.ResizeImage(file_chunk_generator(path, params))
            receive_and_save(stream, "resized_output" + Path(path).suffix)
    except grpc.RpcError as e:
        print(f"[-] Erro na chamada gRPC: {e.details()} (Codigo: {e.code()})")

if __name__ == '__main__':
    main()
