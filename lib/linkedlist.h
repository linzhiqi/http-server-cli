#ifndef NETWORKPROG_LINKEDLIST_H
#define NETWORKPROG_LINKEDLIST_H

#include <pthread.h>

#define MAXFILENAME 100

struct node * fileLinkedList = createFileList();
pthread_rwlock_t fileListLock;

struct node {
  char fileName[MAXFILENAME];
  pthread_rwlock_t * mylock;
  struct node * pre;
  struct node * next;
};
struct node * createFileList();
struct node * appendNode(const char * fileName, struct node * elderNode);
struct node * deleteNode(struct node * nodeToDelete);
struct node * getNode(const char * fileName, struct node * fileList);
void clearFileList(struct node * fileList);

#endif
