#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

// Constants
#define QUEUE_SIZE 10       
#define MAX_THREADS 4        
#define MAX_RANKS 100        
#define MAX_VISITS 20        

// Circular queue data structure for managing URLs
typedef struct {
    char *urls[QUEUE_SIZE];  
    int front, rear, count; 
    pthread_mutex_t lock;    /
    pthread_cond_t not_full, not_empty; 
} CircularQueue;

// Ranking structure to track domains and their visit counts
typedef struct {
    char domain[256];   
    int count;         
} Rank;

// Global variables for domain ranking and visit limits
Rank ranking[MAX_RANKS];       
int rank_count = 0;           
pthread_mutex_t rank_lock;     

int total_visits = 0;          
pthread_mutex_t visit_lock;    

// Function Declarations
void initQueue(CircularQueue *q);
void enqueue(CircularQueue *q, const char *url);
char* dequeue(CircularQueue *q);
void *crawlerThread(void *arg);
void updateRank(const char *domain);
char* parseDomain(const char *url);
int isDuplicate(const char *url);

// Initialize the circular queue
void initQueue(CircularQueue *q) {
    q->front = 0;  
    q->rear = 0;  
    q->count = 0;  
    pthread_mutex_init(&q->lock, NULL); 
    pthread_cond_init(&q->not_full, NULL);  
    pthread_cond_init(&q->not_empty, NULL);
}

// Check if a URL is a duplicate (already visited)
int isDuplicate(const char *url) {
    pthread_mutex_lock(&rank_lock); 
    for (int i = 0; i < rank_count; i++) { 
        if (strcmp(ranking[i].domain, url) == 0) { 
            pthread_mutex_unlock(&rank_lock); 
            return 1; 
        }
    }
    pthread_mutex_unlock(&rank_lock); 
    return 0; 
}

// Enqueue a URL into the circular queue
void enqueue(CircularQueue *q, const char *url) {
    pthread_mutex_lock(&q->lock); 
    while (q->count == QUEUE_SIZE) { 
        pthread_cond_wait(&q->not_full, &q->lock);
    }
    q->urls[q->rear] = strdup(url); 
    q->rear = (q->rear + 1) % QUEUE_SIZE; 
    q->count++; 
    pthread_cond_signal(&q->not_empty); 
    pthread_mutex_unlock(&q->lock); 
}

// Dequeue a URL from the circular queue
char* dequeue(CircularQueue *q) {
    pthread_mutex_lock(&q->lock); 
    while (q->count == 0) { 
        pthread_cond_wait(&q->not_empty, &q->lock);
    }
    char *url = q->urls[q->front]; 
    q->front = (q->front + 1) % QUEUE_SIZE; 
    q->count--; 
    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->lock); 
    return url; 
}

// Update ranking for a domain
void updateRank(const char *domain) {
    pthread_mutex_lock(&rank_lock); 
    for (int i = 0; i < rank_count; i++) { 
        if (strcmp(ranking[i].domain, domain) == 0) { 
            ranking[i].count++; 
            pthread_mutex_unlock(&rank_lock); 
            return;
        }
    }
    // Add new domain if not found
    if (rank_count < MAX_RANKS) {
        strcpy(ranking[rank_count].domain, domain); 
        ranking[rank_count].count = 1; 
        rank_count++; 
    }
    pthread_mutex_unlock(&rank_lock); 
}

// Dummy function to parse domain from a URL
char* parseDomain(const char *url) {
    char *domain = strstr(url, "://"); // Find "://"
    if (domain) domain += 3; // Skip "://"
    else domain = (char*)url; 
    char *end = strchr(domain, '/'); 
    static char result[256]; 
    strncpy(result, domain, end ? (size_t)(end - domain) : strlen(domain));
    result[end ? (end - domain) : strlen(domain)] = '\0'; 
    return result; 
}

// Crawler thread function
void *crawlerThread(void *arg) {
    CircularQueue q = (CircularQueue)arg; 
    while (1) {
        pthread_mutex_lock(&visit_lock); 
        if (total_visits >= MAX_VISITS) { 
            pthread_mutex_unlock(&visit_lock);
            break;
        }
        pthread_mutex_unlock(&visit_lock);

        char *url = dequeue(q);
        if (url == NULL) continue; 

        pthread_mutex_lock(&visit_lock);
        total_visits++; 
        pthread_mutex_unlock(&visit_lock);

        printf("Thread %lu: Crawling URL: %s\n", pthread_self(), url);

        sleep(1); 
        char *domain = parseDomain(url);
        printf("Thread %lu: Parsed Domain: %s\n", pthread_self(), domain);

        updateRank(domain); 

        if (strcmp(url, "http://stop.com") == 0) 
        { 
            free(url);
            break;
        }
        if (!isDuplicate("http://example.com/page"))
        { 
            enqueue(q, "http://example.com/page");
        }
        free(url); 
    }
    return NULL;
}

// Main function
int main() {
    pthread_t threads[MAX_THREADS]; 
    CircularQueue queue; 

    initQueue(&queue);
    pthread_mutex_init(&rank_lock, NULL);
    pthread_mutex_init(&visit_lock, NULL);

    // Seed initial URLs
    enqueue(&queue, "http://example.com");
    enqueue(&queue, "http://another.com");
    enqueue(&queue, "http://stop.com");

    // Create worker threads
    for (int i = 0; i < MAX_THREADS; i++) {
        pthread_create(&threads[i], NULL, crawlerThread, (void*)&queue);
    }

    // Wait for all threads to complete
    for (int i = 0; i < MAX_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    // Print the final domain rankings
    printf("\n--- Domain Rankings ---\n");
    for (int i = 0; i < rank_count; i++) {
        printf("%s: %d visits\n", ranking[i].domain, ranking[i].count);
    }

    // Clean up resources
    pthread_mutex_destroy(&rank_lock);
    pthread_mutex_destroy(&visit_lock);
    pthread_mutex_destroy(&queue.lock);
    pthread_cond_destroy(&queue.not_full);
    pthread_cond_destroy(&queue.not_empty);

    return 0;
}