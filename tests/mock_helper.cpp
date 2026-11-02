#include "model.h"
#include <unistd.h>
#include <thread>
#include <chrono>
int main(int argc,char** argv){if(argc<1)return 2;std::string name=argv[0];if(name.find("crash")!=std::string::npos)return 3;if(name.find("hang")!=std::string::npos){std::this_thread::sleep_for(std::chrono::seconds(60));return 0;}
 uint32_t size=0;if(read(0,&size,4)!=4||size>4096)return 2;std::string header(size,'\0');if(read(0,header.data(),size)!=size)return 2;auto j=dx::Json::parse(header);std::vector<char> pcm(j["samples"].get<int>()*2);size_t received=0;while(received<pcm.size()){auto n=read(0,pcm.data()+received,pcm.size()-received);if(n<=0)return 3;received+=size_t(n);}dx::Json results=dx::Json::array();bool valid=name.find("valid")!=std::string::npos;
 if(name.find("questionable")!=std::string::npos)results.push_back({{"snr",-20},{"audio_hz",1000},{"message","CQ K1BAD AA00"},{"raw","FT8_OUT\t1800000000000\t00\t{0}\t120000\t{1}\t-20\t{2}\t0.1\t{3}\t0\t{4}\tCQ K1BAD AA00|CQ;K1BAD;\t{5}\t? 0\t{6}\t0.1\t{7}\t1000\n"},{"dt",.1}});
 else if(name.find("unresolved")!=std::string::npos)results.push_back({{"snr",-20},{"audio_hz",823},{"message","<1:2303> <2:675541> R 570661 CE81WW"},{"dt",-.2}});
 else if(name.find("unknown")!=std::string::npos){for(const auto& message:{"CQ <1:123> FN42","HELLO, \"WORLD\"","OTHER UNKNOWN TEXT"})results.push_back({{"snr",-12},{"audio_hz",500},{"message",message},{"dt",.1}});}
 else{int count=name.find("small")!=std::string::npos?3:1200;if(valid)for(int n=0;n<count;n++)results.push_back({{"snr",-12},{"audio_hz",500+n},{"message","CQ K"+std::to_string(n)+"ABC FN42"},{"dt",.1}});}
 std::string response=dx::Json{{"id",j["id"].get<int>()+(valid?0:1)},{"results",results}}.dump();size=uint32_t(response.size());write(1,&size,4);write(1,response.data(),size);return 0;
}
