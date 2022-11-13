#设置编译器和编译参数
CC = g++ -std=c++11 -g -Wall 

#设置需要包含的头文件目录
CFLAGS = -I../../include -I../../src -I../ -I../../include/httpparser

#设置需要包含的动态库和静态库目录
LFLAGS = -lpthread

#指定要链接的动态库或静态库
LDEXEFLAGS = 

#设置存放.o文件的目录
OBJDIR = build

#设置存放可执行文件的目录
BINDIR = ../../bin

#设置存放.c和.cpp文件的目录
SRCDIR += . ../../src

#设置可执行文件名字
APP = httpserver

ALL_SRC += $(foreach dir, $(SRCDIR), $(wildcard $(dir)/*.cpp))
ALL_OBJ += $(patsubst $(SRCDIR)/%.cpp, $(OBJDIR)/%.o, $(ALL_SRC))

#设置可执行文件名
all: $(BINDIR)/$(APP)

$(BINDIR)/$(APP): $(ALL_OBJ)
	$(CC) $(CFLAGS) $(ALL_INCLUDE) $(LFLAGS) $(ALL_OBJ) $(LDEXEFLAGS) -o $@

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(OBJDIR)	
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) $(ALL_INCLUDE) $(LFLAGS) -c $< -o $@

#make clean
clean:
	rm -f ./$(OBJDIR)/*.o ./$(BINDIR)/$(APP)
