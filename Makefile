TARGET=prosim

SRC_FILES=main.c context.c prio_q.c process.c barrier.c message.c

all: $(TARGET)

$(TARGET): $(SRC_FILES)
	gcc -Wall -g -o $(TARGET) $(SRC_FILES) -l pthread
