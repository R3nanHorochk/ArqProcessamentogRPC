#include <iostream>
#include <fstream>
#include <memory>
#include <string>
#include <vector>
#include <chrono>
#include <ctime>
#include <cstdlib>
#include <cstdio>
#include <filesystem>

#include <grpcpp/grpcpp.h>
#include "file_processor.grpc.pb.h"

namespace fs = std::filesystem;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReaderWriter;
using grpc::Status;
using file_processor::FileProcessorService;
using file_processor::FileRequest;
using file_processor::FileResponse;
using file_processor::FileChunk;

class FileProcessorServiceImpl final : public FileProcessorService::Service {
private:
    void WriteLog(const std::string& level, const std::string& service, const std::string& file, const std::string& msg) {
        auto now = std::chrono::system_clock::now();
        std::time_t now_c = std::chrono::system_clock::to_time_t(now);
        std::tm now_tm;
        localtime_r(&now_c, &now_tm);

        char ts[32];
        std::strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &now_tm);

        std::ofstream log_file("server.log", std::ios::app);
        if (log_file.is_open()) {
            log_file << "[" << ts << "] [" << level << "] Service: " << service 
                     << " | File: " << file << " | Message: " << msg << std::endl;
        }
        std::cout << "[" << ts << "] [" << level << "] Service: " << service 
                  << " | File: " << file << " | Message: " << msg << std::endl;
    }

    bool StreamFileToClient(ServerReaderWriter<FileResponse, FileRequest>* stream, 
                           const std::string& out_path, 
                           const std::string& out_name, 
                           const std::string& success_msg) {
        std::ifstream out_file(out_path, std::ios::binary);
        if (!out_file) return false;

        char buffer[64 * 1024];
        bool first = true;
        while (out_file.read(buffer, sizeof(buffer)) || out_file.gcount() > 0) {
            FileResponse res;
            if (first) {
                res.set_file_name(out_name);
                res.set_success(true);
                res.set_status_message(success_msg);
                first = false;
            }
            res.mutable_chunk()->set_content(buffer, out_file.gcount());
            stream->Write(res);
        }
        return true;
    }

public:
    Status CompressPDF(ServerContext* context, 
                       ServerReaderWriter<FileResponse, FileRequest>* stream) override {
        FileRequest req;
        std::string file_name = "document.pdf";
        std::string temp_in = "/tmp/in_" + std::to_string(rand()) + ".pdf";
        std::string temp_out = "/tmp/out_" + std::to_string(rand()) + ".pdf";

        std::ofstream in_file(temp_in, std::ios::binary);
        while (stream->Read(&req)) {
            if (!req.file_name().empty()) file_name = req.file_name();
            if (req.has_chunk()) {
                in_file.write(req.chunk().content().data(), req.chunk().content().size());
            }
        }
        in_file.close();

        std::string cmd = "gs -sDEVICE=pdfwrite -dCompatibilityLevel=1.4 -dPDFSETTINGS=/ebook -dNOPAUSE -dQUIET -dBATCH -sOutputFile=" + temp_out + " " + temp_in;
        int ret = std::system(cmd.c_str());

        if (ret == 0 && fs::exists(temp_out)) {
            WriteLog("SUCCESS", "CompressPDF", file_name, "PDF comprimido com sucesso.");
            StreamFileToClient(stream, temp_out, "compressed_" + file_name, "Compressao bem sucedida");
        } else {
            WriteLog("ERROR", "CompressPDF", file_name, "Falha Ghostscript. Cod: " + std::to_string(ret));
            FileResponse err_res;
            err_res.set_success(false);
            err_res.set_status_message("Falha ao comprimir PDF.");
            stream->Write(err_res);
        }

        std::remove(temp_in.c_str());
        std::remove(temp_out.c_str());
        return Status::OK;
    }

    Status ConvertToTXT(ServerContext* context, 
                        ServerReaderWriter<FileResponse, FileRequest>* stream) override {
        FileRequest req;
        std::string file_name = "document.pdf";
        std::string temp_in = "/tmp/in_" + std::to_string(rand()) + ".pdf";
        std::string temp_out = "/tmp/out_" + std::to_string(rand()) + ".txt";

        std::ofstream in_file(temp_in, std::ios::binary);
        while (stream->Read(&req)) {
            if (!req.file_name().empty()) file_name = req.file_name();
            if (req.has_chunk()) {
                in_file.write(req.chunk().content().data(), req.chunk().content().size());
            }
        }
        in_file.close();

        std::string cmd = "pdftotext " + temp_in + " " + temp_out;
        int ret = std::system(cmd.c_str());

        if (ret == 0 && fs::exists(temp_out)) {
            WriteLog("SUCCESS", "ConvertToTXT", file_name, "Conversao PDF para TXT concluida.");
            StreamFileToClient(stream, temp_out, fs::path(file_name).stem().string() + ".txt", "TXT gerado");
        } else {
            WriteLog("ERROR", "ConvertToTXT", file_name, "Falha pdftotext. Cod: " + std::to_string(ret));
            FileResponse err_res;
            err_res.set_success(false);
            err_res.set_status_message("Falha ao converter PDF para TXT.");
            stream->Write(err_res);
        }

        std::remove(temp_in.c_str());
        std::remove(temp_out.c_str());
        return Status::OK;
    }

    Status ConvertImageFormat(ServerContext* context, 
                              ServerReaderWriter<FileResponse, FileRequest>* stream) override {
        FileRequest req;
        std::string file_name = "image.bin";
        std::string out_fmt = "png";
        std::string temp_in = "/tmp/in_" + std::to_string(rand()) + ".bin";
        std::string temp_out = "/tmp/out_" + std::to_string(rand());

        std::ofstream in_file(temp_in, std::ios::binary);
        while (stream->Read(&req)) {
            if (!req.file_name().empty()) file_name = req.file_name();
            if (req.has_convert_image_format_params()) {
                out_fmt = req.convert_image_format_params().output_format();
            }
            if (req.has_chunk()) {
                in_file.write(req.chunk().content().data(), req.chunk().content().size());
            }
        }
        in_file.close();

        temp_out += "." + out_fmt;
        std::string cmd = "convert " + temp_in + " " + temp_out;
        int ret = std::system(cmd.c_str());

        if (ret == 0 && fs::exists(temp_out)) {
            WriteLog("SUCCESS", "ConvertImageFormat", file_name, "Formato alterado para " + out_fmt);
            StreamFileToClient(stream, temp_out, fs::path(file_name).stem().string() + "." + out_fmt, "Convertida");
        } else {
            WriteLog("ERROR", "ConvertImageFormat", file_name, "Falha convert. Cod: " + std::to_string(ret));
            FileResponse err_res;
            err_res.set_success(false);
            err_res.set_status_message("Erro convertendo imagem.");
            stream->Write(err_res);
        }

        std::remove(temp_in.c_str());
        std::remove(temp_out.c_str());
        return Status::OK;
    }

    Status ResizeImage(ServerContext* context, 
                       ServerReaderWriter<FileResponse, FileRequest>* stream) override {
        FileRequest req;
        std::string file_name = "image.bin";
        int w = 800, h = 600;
        std::string temp_in = "/tmp/in_" + std::to_string(rand()) + ".bin";
        std::string temp_out = "/tmp/out_" + std::to_string(rand()) + ".bin";

        std::ofstream in_file(temp_in, std::ios::binary);
        while (stream->Read(&req)) {
            if (!req.file_name().empty()) file_name = req.file_name();
            if (req.has_resize_image_params()) {
                w = req.resize_image_params().width();
                h = req.resize_image_params().height();
            }
            if (req.has_chunk()) {
                in_file.write(req.chunk().content().data(), req.chunk().content().size());
            }
        }
        in_file.close();

        std::string cmd = "convert " + temp_in + " -resize " + std::to_string(w) + "x" + std::to_string(h) + "\\! " + temp_out;
        int ret = std::system(cmd.c_str());

        if (ret == 0 && fs::exists(temp_out)) {
            WriteLog("SUCCESS", "ResizeImage", file_name, "Redimensionada com sucesso.");
            StreamFileToClient(stream, temp_out, "resized_" + file_name, "Imagem redimensionada");
        } else {
            WriteLog("ERROR", "ResizeImage", file_name, "Erro ao redimensionar. Cod: " + std::to_string(ret));
            FileResponse err_res;
            err_res.set_success(false);
            err_res.set_status_message("Falha ao redimensionar.");
            stream->Write(err_res);
        }

        std::remove(temp_in.c_str());
        std::remove(temp_out.c_str());
        return Status::OK;
    }
};

int main() {
    std::string server_address("0.0.0.0:50051");
    FileProcessorServiceImpl service;
    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Servidor gRPC C++ ouvindo na porta 50051..." << std::endl;
    server->Wait();
    return 0;
}
