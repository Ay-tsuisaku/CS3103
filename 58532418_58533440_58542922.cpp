#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <cstring>
#include <cmath>

using namespace std;

#define CACHE_SIZE 5
#define FRAME_LEN 8
#define MAX_FRAMES 10

// External function declarations
extern "C" {
    double* generate_frame_vector(int length);
    double* compression(double* frame, int length);
}

struct QueueEntry {
    double* original_frame;
    double* compressed_frame;
};

struct Queue {
    QueueEntry queue[CACHE_SIZE];
    int front, rear, count;

    bool full() {return count == CACHE_SIZE;}
    bool empty() {return count == 0;}

    bool enqueue(double* frame) {
        if (full()) return false;
        queue[rear].original_frame = frame;
        queue[rear].compressed_frame = NULL;
        rear = (rear + 1) % CACHE_SIZE;
        count++;
        return true;
    }
    double* dequeue() {
        if (empty()) return NULL;
        double* output = queue[front].original_frame;
        front = (front + 1) % CACHE_SIZE;
        count--;
        return output;
    }
    double* get_noDequeue() {
        if (empty()) return NULL;
        return queue[front].original_frame;
    }
    double* set_compressed(double* compressed) {
        if (!empty()) {
            queue[front].compressed_frame = compressed;
        }
        return compressed;
    }
};

Queue frame_cache;

sem_t cache_emptied;
sem_t cache_loaded;
sem_t transformer_loaded;
sem_t mse_loaded;
double temp[FRAME_LEN];

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

double calculate_mse(const double* a, const double* b, int len) {
    double mse = 0.0;
    for (int i = 0; i < len; ++i)
        mse += (a[i] - b[i]) * (a[i] - b[i]);
    mse /= len;
    return mse;
}

void* camera(void* arg) {
    int INTERVAL_SECONDS = (int)(intptr_t)arg;
    int frames_generated = 0;
    
    while(frames_generated < MAX_FRAMES) {
        double* frame = generate_frame_vector(FRAME_LEN);
        if (!frame) break;
        
        sem_wait(&cache_emptied);
        
        pthread_mutex_lock(&mutex);
        frame_cache.enqueue(frame);
        pthread_mutex_unlock(&mutex);

        sem_post(&cache_loaded);
        
        sleep(INTERVAL_SECONDS);
        frames_generated++;
    }
    return NULL;
}

void* transformer(void* arg) {
    int frames_compressed = 0;
    
    while(frames_compressed < MAX_FRAMES) {
        sem_wait(&cache_loaded);
        
        pthread_mutex_lock(&mutex);
        if (frame_cache.empty()) {
            pthread_mutex_unlock(&mutex);
            break;
        }
        
        double* original = frame_cache.get_noDequeue();
        memcpy(temp, original, FRAME_LEN * sizeof(double));
        compression(temp, FRAME_LEN);
        
        pthread_mutex_unlock(&mutex);
        
        sleep(3);
        sem_post(&transformer_loaded);
        frames_compressed++;
    }
    return NULL;
}

void* estimator(void* arg) {
    int frames_estimated = 0;
    
    while(frames_estimated < MAX_FRAMES) {
        sem_wait(&transformer_loaded);
        
        pthread_mutex_lock(&mutex);
        if (frame_cache.empty()) {
            pthread_mutex_unlock(&mutex);
            break;
        }
        
        double* original = frame_cache.get_noDequeue();
        double mse = calculate_mse(original, temp, FRAME_LEN);
        printf("mse = %f\n", mse);

        double* frame_to_free = frame_cache.dequeue();
        free(frame_to_free);
        
        pthread_mutex_unlock(&mutex);
        
        sem_post(&cache_emptied);
        frames_estimated++;
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Invalid number of arguments.");
        return 0;
    }
    int INTERVAL_SECONDS = atoi(argv[1]);

    sem_init(&cache_loaded, 0, 0);
    sem_init(&cache_emptied, 0, CACHE_SIZE);
    sem_init(&transformer_loaded, 0, 0);
    sem_init(&mse_loaded, 0, 0);

    pthread_t camera_thread, transformer_thread, estimator_thread;

    pthread_create(&camera_thread, NULL, camera, (void *)(intptr_t)INTERVAL_SECONDS);
    pthread_create(&transformer_thread, NULL, transformer, NULL);
    pthread_create(&estimator_thread, NULL, estimator, NULL);

    pthread_join(camera_thread, NULL);
    pthread_join(transformer_thread, NULL);
    pthread_join(estimator_thread, NULL);

    sem_destroy(&cache_loaded);
    sem_destroy(&cache_emptied);
    sem_destroy(&transformer_loaded);
    sem_destroy(&mse_loaded);

    pthread_mutex_destroy(&mutex);

    return 0;
}


