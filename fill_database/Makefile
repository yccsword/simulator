CC = gcc 
LIB_PATH = /usr/local/lib -L../mysql/lib
#MySQL
MYSQL_INCLUDE = -I../mysql/include
CFLAGS += $(MYSQL_INCLUDE)
LDFLAGS += -lmysqlclient
#GDB
CFLAGS += -g -ggdb

RM = /bin/rm -f 

FILL_DATABASE_OBJS = fill_database.o cfg.o

FILL_DATABASE_SRCS = $(FILL_DATABASE_OBJS:.o=.c)

FILL_DATABASE_NAME = fill_database

.PHONY: clean

all: $(FILL_DATABASE_NAME)

$(FILL_DATABASE_NAME): $(FILL_DATABASE_OBJS) 
	$(CC) $(FILL_DATABASE_OBJS) $(CC_FLAGS) -L$(LIB_PATH) $(LDFLAGS) -o $(FILL_DATABASE_NAME) 
	
clean: 
	$(RM) $(FILL_DATABASE_NAME) $(FILL_DATABASE_OBJS)
