#include <iostream>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <cstdlib>
#include <cstring>

extern "C" {
    double* generate_frame_vector(int length);
    void compression(double* frame, int length);
}

#define FRAME_LEN 8
#define CACHE_SIZE 5
#define MAX_FRAMES 10

using namespace std;

struct QueueEntry {
    double* original_frame;
    double* compressed_frame;
};

class FrameQueue {
public:
    QueueEntry queue[CACHE_SIZE];
    int front, rear, count;
    pthread_mutex_t mutex;

    FrameQueue() {
        front = 0;
        rear = 0;
        count = 0;
        pthread_mutex_init(&mutex, NULL);
    }

    bool is_full() { return count == CACHE_SIZE; }
    bool is_empty() { return count == 0; }

    void enqueue(double* frame) {
        queue[rear].original_frame = frame;
        queue[rear].compressed_frame = NULL;
        rear = (rear + 1) % CACHE_SIZE;
        count++;
    }

    void set_compressed(double* compressed) {
        queue[front].compressed_frame = compressed;
    }

    QueueEntry get_front_entry() {
        return queue[front];
    }

    void dequeue() {
        front = (front + 1) % CACHE_SIZE;
        count--;
    }
};

FrameQueue frame_cache;
sem_t cache_emptied;
sem_t cache_loaded;
sem_t transformer_loaded;
bool camera_done = false;

double calculate_mse(double* original, double* compressed, int length) {
    double mse = 0.0;
    for (int i = 0; i < length; i++) {
        double diff = original[i] - compressed[i];
        mse += diff * diff;
    }
    return mse / length;
}

void* camera_thread(void* arg) {
    int interval = *((int*)arg);
    int frames_generated = 0;
    while (frames_generated < MAX_FRAMES) {
        double* frame = generate_frame_vector(FRAME_LEN);
        if (!frame) break;

        sem_wait(&cache_emptied);
        pthread_mutex_lock(&frame_cache.mutex);

        frame_cache.enqueue(frame);

        pthread_mutex_unlock(&frame_cache.mutex);
        sem_post(&cache_loaded);

        sleep(interval);
        frames_generated++;
    }
    camera_done = true;
    sem_post(&cache_loaded);
    sem_post(&transformer_loaded);
    return NULL;
}

void* transformer_thread(void* arg) {
    while (true) {
        if (camera_done && frame_cache.is_empty()) break;

        sem_wait(&cache_loaded);
        pthread_mutex_lock(&frame_cache.mutex);

        if (frame_cache.is_empty()) {
            pthread_mutex_unlock(&frame_cache.mutex);
            continue;
        }

        double* original = frame_cache.get_front_entry().original_frame;
        double* compressed = (double*)malloc(FRAME_LEN * sizeof(double));
        memcpy(compressed, original, FRAME_LEN * sizeof(double));
        compression(compressed, FRAME_LEN);
        frame_cache.set_compressed(compressed);

        pthread_mutex_unlock(&frame_cache.mutex);
        sem_post(&transformer_loaded);

        sleep(3);
    }
    return NULL;
}

void* estimator_thread(void* arg) {
    while (true) {
        if (camera_done && frame_cache.is_empty()) break;

        sem_wait(&transformer_loaded);
        pthread_mutex_lock(&frame_cache.mutex);

        if (frame_cache.is_empty()) {
            pthread_mutex_unlock(&frame_cache.mutex);
            continue;
        }

        QueueEntry entry = frame_cache.get_front_entry();
        if (entry.compressed_frame == NULL) {
            pthread_mutex_unlock(&frame_cache.mutex);
            continue;
        }

        double mse = calculate_mse(entry.original_frame, entry.compressed_frame, FRAME_LEN);
        printf("mse = %f\n", mse);

        free(entry.original_frame);
        free(entry.compressed_frame);
        frame_cache.dequeue();

        pthread_mutex_unlock(&frame_cache.mutex);
        sem_post(&cache_emptied);
    }
    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Usage: %s <interval_seconds>\n", argv[0]);
        return 1;
    }

    int interval = atoi(argv[1]);

    sem_init(&cache_emptied, 0, CACHE_SIZE);
    sem_init(&cache_loaded, 0, 0);
    sem_init(&transformer_loaded, 0, 0);

    pthread_t camera, transformer, estimator;
    pthread_create(&camera, NULL, camera_thread, &interval);
    pthread_create(&transformer, NULL, transformer_thread, NULL);
    pthread_create(&estimator, NULL, estimator_thread, NULL);

    pthread_join(camera, NULL);
    pthread_join(transformer, NULL);
    pthread_join(estimator, NULL);

    pthread_mutex_destroy(&frame_cache.mutex);
    sem_destroy(&cache_emptied);
    sem_destroy(&cache_loaded);
    sem_destroy(&transformer_loaded);

    return 0;
}
