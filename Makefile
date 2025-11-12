ALL = 58532418_58533440_58542922

all: $(ALL)

clean:
	rm -f $(ALL) *.o *~

$(ALL): 58532418_58533440_58542922.o compression.o generate_frame_vector.o
	g++ 58532418_58533440_58542922.o compression.o generate_frame_vector.o -lpthread -lm -o $(ALL)

58532418_58533440_58542922.o: 58532418_58533440_58542922.cpp
	g++ -c 58532418_58533440_58542922.cpp

compression.o: compression.c
	cc -c compression.c

generate_frame_vector.o: generate_frame_vector.c
	gcc -c generate_frame_vector.c
