#include "api.hpp"
#include <crow.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>

namespace {
struct Boundary {
    struct context {};
    unsigned port=8080;
    std::string public_host;
    void before_handle(crow::request& request,crow::response& response,context&) {
        const auto& host=request.get_header_value("Host");
        const auto& origin=request.get_header_value("Origin");
        bool allowed=host.rfind("127.0.0.1:",0)==0 || host.rfind("localhost:",0)==0 ||
            (!public_host.empty() && (host==public_host || host==public_host+":"+std::to_string(port)));
        if(!allowed || (!origin.empty() && origin!="http://"+host)) {
            response.code=403;response.body=R"({"error":{"code":"origin","message":"Use the local application origin"}})";response.end();return;
        }
        if((request.method==crow::HTTPMethod::Post || request.method==crow::HTTPMethod::Put) && request.get_header_value("Content-Type").find("application/json")!=0) {
            response.code=415;response.body=R"({"error":{"code":"content_type","message":"Use application/json"}})";response.end();
        }
    }
    void after_handle(crow::request&,crow::response& response,context&) {
        response.set_header("Cache-Control","no-store");
        response.set_header("X-Content-Type-Options","nosniff");
        response.set_header("Referrer-Policy","no-referrer");
        response.set_header("X-Frame-Options","DENY");
        response.set_header("Content-Security-Policy","default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; object-src 'none'; base-uri 'none'; frame-ancestors 'none'; form-action 'none'");
    }
};
std::string read_file(const std::filesystem::path& path) {
    std::ifstream stream(path,std::ios::binary);
    if(!stream) throw std::runtime_error("Cannot open "+path.string()+"; pass --root /path/to/project");
    std::ostringstream content;content<<stream.rdbuf();return content.str();
}
}
int main(int argc,char** argv) {
    try {
        std::filesystem::path root=SMART_SOURCE_DIR;
        unsigned port=8080;std::string host="127.0.0.1",public_host;smart::Limits limits;
        for(int i=1;i<argc;++i) {
            std::string option=argv[i];
            if(option=="--help") {std::cout<<"smartautocomplete [--root PATH] [--port 8080] [--max-words N] [--max-text N] [--max-pattern N] [--max-events N] [--max-body N] [--max-trace-bytes N]\n";return 0;}
            if(i+1>=argc) throw std::runtime_error("Missing value for "+option);
            std::string value=argv[++i];
            if(option=="--root") {root=value;continue;}
            if(option=="--host") {host=value;continue;}
            if(option=="--public-host") {public_host=value;continue;}
            std::size_t consumed=0;auto number=std::stoull(value,&consumed);
            if(consumed!=value.size() || number==0 || value.front()=='-') throw std::runtime_error("Expected positive integer for "+option);
            if(option=="--port") {if(number>65535) throw std::runtime_error("Invalid port");port=static_cast<unsigned>(number);}
            else if(option=="--max-words") limits.words=number;
            else if(option=="--max-text") limits.text=number;
            else if(option=="--max-pattern") limits.pattern=number;
            else if(option=="--max-events") limits.events=number;
            else if(option=="--max-body") limits.body=number;
            else if(option=="--max-trace-bytes") limits.trace_bytes=number;
            else throw std::runtime_error("Unknown option: "+option);
        }
        std::vector<std::string> words;std::istringstream dictionary(read_file(root/"data/words.txt"));
        for(std::string line;std::getline(dictionary,line);) {if(!line.empty() && line.back()=='\r') line.pop_back();if(!line.empty()) words.push_back(line);}
        smart::Api api(words,limits);
        std::map<std::string,std::pair<std::string,std::string>> files;
        files["/"]={read_file(root/"static/index.html"),"text/html; charset=utf-8"};
        files["style.css"]={read_file(root/"static/style.css"),"text/css; charset=utf-8"};
        files["app.js"]={read_file(root/"static/app.js"),"application/javascript; charset=utf-8"};
        crow::App<Boundary> app;
        app.get_middleware<Boundary>().port=port;app.get_middleware<Boundary>().public_host=public_host;
        auto asset=[&](const std::string& key) {
            auto it=files.find(key);
            if(it==files.end()) return crow::response(404);
            crow::response response(it->second.first);response.set_header("Content-Type",it->second.second);return response;
        };
        CROW_ROUTE(app,"/")([&]{return asset("/");});
        CROW_ROUTE(app,"/static/<string>")([&](const std::string& file){return asset(file);});
        CROW_ROUTE(app,"/health")([&]{auto value=api.handle("GET","/health");crow::response response(value.status,value.body);response.set_header("Content-Type","application/json");return response;});
        CROW_ROUTE(app,"/api/<string>").methods(crow::HTTPMethod::Get,crow::HTTPMethod::Post,crow::HTTPMethod::Put)([&](const crow::request& request,const std::string& endpoint) {
            std::string method=request.method==crow::HTTPMethod::Get?"GET":request.method==crow::HTTPMethod::Put?"PUT":"POST";
            auto result=api.handle(method,"/api/"+endpoint,request.body);
            crow::response response(result.status,result.body);response.set_header("Content-Type","application/json; charset=utf-8");return response;
        });
        std::cout<<"SmartAutocomplete: http://127.0.0.1:"<<port<<" (offline, single-user)\n";
        app.loglevel(crow::LogLevel::Warning).bindaddr(host).port(static_cast<std::uint16_t>(port)).concurrency(2).timeout(15).run();
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
