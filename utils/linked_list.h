#pragma once

#include <stdio.h>
#include <stdlib.h>

typedef enum {
	LL_OK = 0,
	LL_BAD_INDEX = -1,
	LL_ALLOC_FAIL = -2,
	LL_EMPTY = -3,
	LL_NULL = -4
}LL_STATUS;

typedef struct Node {
	void *data;
	struct Node *next;
} Node;

typedef struct LinkedList {
	Node *head;
	size_t size;
} LinkedList;

void ll_init(LinkedList *list) {
	list->head = NULL;
	list->size = 0;
}

LL_STATUS ll_free(LinkedList *list) {
	if (!list) return LL_NULL;

	Node *current = list->head;
	while (current != NULL) {
		Node *next = current->next;
		free(current);
		current = next;
	}
	list->head = NULL;
	list->size = 0;
	return LL_OK;
}

LL_STATUS ll_append(LinkedList *list, void *data) {
	if (!list) return LL_NULL;

	Node *new_node = malloc(sizeof(Node));
	if (!new_node) return LL_ALLOC_FAIL;

	new_node->data = data;
	new_node->next = NULL;
	if (list->head == NULL) {
		list->head = new_node;
	}
	else {
		Node *current = list->head;
		while (current->next != NULL) {
			current = current->next;
		}
		current->next = new_node;
	}
	list->size++;
	return LL_OK;
}

void *ll_get(LinkedList *list, const size_t index) {
	if (!list) return NULL;
	if (index >= list->size) return NULL;
	Node *current = list->head;
	for (size_t i = 0; i < index; i++) {
		current = current->next;
	}
	return current->data;
}

LL_STATUS ll_remove(LinkedList *list, const size_t index) {
	if (!list) return LL_NULL;
	if (index >= list->size) return LL_BAD_INDEX;
	Node *current = list->head;
	Node *prev = NULL;
	for (size_t i = 0; i < index; i++) {
		prev = current;
		current = current->next;
	}
	if (prev == NULL) {
		list->head = current->next;
	}
	else {
		prev->next = current->next;
	}
	free(current);
	list->size--;
	return LL_OK;
}

LL_STATUS ll_insert(LinkedList *list, const size_t index, void *data) {
	if (!list) return LL_NULL;
	if (index > list->size) return LL_BAD_INDEX;
	Node *new_node = malloc(sizeof(Node));
	if (!new_node) return LL_ALLOC_FAIL;
	new_node->data = data;
	new_node->next = NULL;
	if (index == 0) {
		new_node->next = list->head;
		list->head = new_node;
	}
	else {
		Node *current = list->head;
		for (size_t i = 0; i < index - 1; i++) {
			current = current->next;
		}
		new_node->next = current->next;
		current->next = new_node;
	}
	list->size++;
	return LL_OK;
}