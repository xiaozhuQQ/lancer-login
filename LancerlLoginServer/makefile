
CXX = g++
CPPFLAGS += -I/usr/local/include -I.  -pthread
CXXFLAGS += -std=c++11
LDFLAGS += -L/usr/local/lib `pkg-config --libs grpc++`            \
           -lgrpc++_reflection \
		   -lmysqlcppconn\
           -lprotobuf -lpthread -ldl
PROTOC = protoc
GRPC_CPP_PLUGIN = grpc_cpp_plugin
GRPC_CPP_PLUGIN_PATH ?= `which $(GRPC_CPP_PLUGIN)`

PROTOS_PATH = ../../protos

vpath %.proto $(PROTOS_PATH)

all: client server

client_l: cpp/lancerLogin.pb.o cpp/lancerLogin.grpc.pb.o client/client_test.o 
	$(CXX) $^ $(LDFLAGS) -o $@

client_ll: cpp/lancerLogin.pb.o cpp/lancerLogin.grpc.pb.o client/client_test.o 
	$(CXX) $^ $(LDFLAGS) -o $@

server_l: cpp/lancerLogin.pb.o cpp/lancerLogin.grpc.pb.o server/LancerServer.o server/DBManager.o utils/MD5.o
	$(CXX) $^ $(LDFLAGS) -o $@

%.grpc.pb.cc: %.proto
	$(PROTOC) -I $(PROTOS_PATH) --grpc_out=. --plugin=protoc-gen-grpc=$(GRPC_CPP_PLUGIN_PATH) $<

%.pb.cc: %.proto
	$(PROTOC) -I $(PROTOS_PATH) --cpp_out=. $<

clean:
	rm -f *.o *.pb.cc *.pb.h lancerLogin_client lancerLogin_server