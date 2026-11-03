CC       = gcc
CXX      = g++

ifeq ($(OS),Windows_NT)
CXXFLAGS=-std=c++11 -fexceptions -DUNICODE -D_UNICODE -DNO_GUI -fPIC -fpermissive 
CFLAGS= -fPIC -Wno-enum-conversion -O3
EXEC_NAME=NxNandManager.exe
INCLUDES=-Ivirtual_fs/dokan/include 
LIBS=-static -lcrypto -lwsock32 -lws2_32 -Lvirtual_fs/dokan/x86/lib -ldokan1
VFS_SRC = $(filter-out virtual_fs/virtual_fs_stub.cpp, $(wildcard virtual_fs/*.cpp))
RC_SRC = $(wildcard gui/*.rc)
else
# Linux (CLI only, see gui/NxNandManager.pro for the GUI)
CXXFLAGS=-std=c++17 -O2 -fexceptions -DNO_GUI -D_7ZIP_ST -fpermissive -pthread -Wno-write-strings -Wno-narrowing
CFLAGS= -O2 -Wno-enum-conversion -D_7ZIP_ST -DS_OK=0
EXEC_NAME=NxNandManager
INCLUDES=
LIBS=-pthread -lcrypto
VFS_SRC = virtual_fs/virtual_fs_stub.cpp
RC_SRC =
endif

# Sources of external libraries
SRC_ZLIB  = $(wildcard lib/ZipLib/extlibs/zlib/*.c)
SRC_LZMA  = $(wildcard lib/ZipLib/extlibs/lzma/*.c)
ifneq ($(OS),Windows_NT)
# Multi-threaded LZMA code is Win32 only
SRC_LZMA := $(filter-out %/LzFindMt.c %/MtCoder.c %/Threads.c, $(SRC_LZMA))
endif
SRC_BZIP2 = $(wildcard lib/ZipLib/extlibs/bzip2/*.c)
SRC_FATFS = $(wildcard lib/fatfs/*.cpp)

# sources
SRC = \
	$(wildcard *.cpp)        \
	$(wildcard res/*.cpp)        \
	$(VFS_SRC)        \
	$(wildcard lib/ZipLib/*.cpp)        \
	$(wildcard lib/ZipLib/detail/*.cpp)

RCOBJ = $(patsubst %.rc,%.rc.o, $(RC_SRC))
OBJS = $(RCOBJ) \
	$(SRC:.cpp=.o)     \
	$(SRC_ZLIB:.c=.o)  \
	$(SRC_LZMA:.c=.o)  \
	$(SRC_BZIP2:.c=.o) \
	$(SRC_FATFS:.cpp=.o)     \
	res/hactool/utils.o

INSTALL_DIR="/build"

all : $(EXEC_NAME)

clean :
	-@rm -rf *.o
	-@rm -rf gui/NxNandManager.tres
	-@rm -rf res/*.o
	-@rm -rf lib/ZipLib/*.o
	-@rm -rf lib/ZipLib/detail/*.o
	-@rm -rf lib/ZipLib/extlibs/zlib/*.o
	-@rm -rf lib/ZipLib/extlibs/lzma/*.o
	-@rm -rf lib/ZipLib/extlibs/bzip2/*.o
	-@rm -rf lib/fatfs/*.o
	-@rm -rf virtual_fs/*.o
	-@rm -rf res/hactool/*.o
	-@rm -rf $(EXEC_NAME)

$(EXEC_NAME): $(OBJS)
	$(CXX) -o $@ $^ $(LIBS) 

$(RCOBJ) : %.rc.o : %.rc
	windres $^ -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

install :
	cp $(EXEC_NAME) $(INSTALL_DIR)