#include <stdio.h>
#include <stdlib.h>
#include "linkedlist.h"
#include <string.h>


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
  /*pthread_rwlock_wrlock(&fileListLock);*/
  newNode->pre = elderNode;
  newNode->next = elderNode->next;
  elderNode->next = newNode;
  /*pthread_rwlock_unlock(&fileListLock);*/
  return newNode;
}

struct node * deleteNode(struct node * nodeToDelete)
{
  /*pthread_rwlock_wrlock(&fileListLock);*/
  struct node * tmp = nodeToDelete->pre;
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
  printf("debug: gonna free node: %s\n",nodeToDelete->fileName);
  free(nodeToDelete);
  /*pthread_rwlock_unlock(&fileListLock);*/
  return tmp;
}

struct node * getNode(const char * fileName, struct node * fileList)
{
  /*pthread_rwlock_rdlock(&fileListLock);*/
  struct node * tmp = fileList;
  while(tmp != NULL)
  {
    if(strncmp(fileName, tmp->fileName, MAXFILENAME) == 0)
    {
      return tmp;
    }else{
      tmp = tmp->next;
    }
  }
  pthread_rwlock_unlock(&fileListLock);
  return NULL;
}

void clearFileList(struct node * fileList)
{
  pthread_rwlock_wrlock(&fileListLock);
  while(fileList->next != NULL)
  {
    deleteNode(fileList->next);
  }
  free(fileList);
  pthread_rwlock_unlock(&fileListLock);
  pthread_rwlock_destroy(&fileListLock);
}
