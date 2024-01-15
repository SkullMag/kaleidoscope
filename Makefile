CC = clang++
SRC = lang.cpp src/*.cpp
CPP_INCLUDE = -I/opt/homebrew/Cellar/llvm/17.0.6/include -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS
CPP_LIB_SEARCH_PATH = -L/opt/homebrew/Cellar/llvm/17.0.6/lib -Wl,-search_paths_first -Wl,-headerpad_max_install_names
CPP_LD = -lLLVM-17
CPP_STD = -std=c++20

all:
	$(CC) $(SRC) $(CPP_INCLUDE) $(CPP_LIB_SEARCH_PATH) $(CPP_LD) $(CPP_STD)