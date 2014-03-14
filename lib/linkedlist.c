#include <stdio.h>
#include <stdlib.h>
#include "linkedlist.h"
#include <string.h>
#include "logutil.h"

struct node * fileLinkedList;
pthread_rwlock_t fileListLock;

void init_file_list(){
    if(fileLinkedList==NULL)
        fileLinkedList=createFileList();
}

struct node * createFileList()
{
  struct node *fileList;
  pthread_rwlock_init(&fileListLock, NULL);
  fileList = (struct node * )malloc(sizeof(struct node));
  fileList->mylock = NULL;
  fileList->pre = NULL;
  fileList->next = NULL;
  return fileList;
}

struct node * appendNode(const char * fileName, struct node * elderNode)
{
  struct node *newNode;
  newNode = (struct node *)malloc(sizeof(struct node));
  strncpy(newNode->fileName, fileName, MAXFILENAME);
  newNode->mylock = (pthread_rwlock_t *)malloc(sizeof(pthread_rwlock_t));
  pthread_rwlock_init(newNode->mylock, NULL);
  log_debug("about to write-lock list\n");
  pthread_rwlock_wrlock(&fileListLock);
  log_debug("has write-lock list\n");
  newNode->pre = elderNode;
  newNode->next = elderNode->next;
  elderNode->next = newNode;
  pthread_rwlock_unlock(&fileListLock);
  log_debug("has unlock list\n");
  return newNode;
}

struct node * deleteNode(struct node * nodeToDelete)
{
  struct node * tmp;
  /*wait until no thread are holding the lock of the list*/
  pthread_rwlock_wrlock(&fileListLock);
  /*wait until no thread are holding the lock of the file*/
  if(nodeToDelete->mylock != NULL){
    pthread_rwlock_wrlock(nodeToDelete->mylock);
    pthread_rwlock_unlock(nodeToDelete->mylock);
  }
  tmp = nodeToDelete->pre;
  tmp->next = nodeToDelete->next;
  if(nodeToDelete->next != NULL)
  {
    nodeToDelete->next->pre = tmp;
  }
  if(nodeToDelete->mylock != NULL)
  {
    pthread_rwlock_destroy(nodeToDelete->mylock);
    free(nodeToDelete->mylock);
  }
  log_debug("gonna free node: %s\n",nodeToDelete->fileName);
  free(nodeToDelete);
  pthread_rwlock_unlock(&fileListLock);
  return tmp;
}

struct node * getNode(const char * fileName, struct node * fileList)
{
  struct node * tmp = fileList;
  log_debug("about to write-lock list\n");
  pthread_rwlock_rdlock(&fileListLock);
  log_debug("has write-lock list\n");
  while(tmp != NULL)
  {
    if(strncmp(fileName, tmp->fileName, MAXFILENAME) == 0)
    {
      pthread_rwlock_unlock(&fileListLock);
      log_debug("has unlock list\n");
      return tmp;
    }else{
      tmp = tmp->next;
    }
  }
  pthread_rwlock_unlock(&fileListLock);
  log_debug("has unlock list\n");
  return NULL;
}

void clearFileList(struct node * fileList)
{
  if(fileList==NULL) return;
  while(fileList->next != NULL)
  {
    deleteNode(fileList->next);
  }
  free(fileList);
  pthread_rwlock_destroy(&fileListLock);
}
