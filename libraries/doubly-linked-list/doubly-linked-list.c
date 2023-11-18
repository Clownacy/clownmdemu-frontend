#include "doubly-linked-list.h"

#include <stddef.h>

void DoublyLinkedList_Initialise(DoublyLinkedList* const list)
{
	list->head = NULL;
	list->tail = NULL;
}

void DoublyLinkedList_PushFront(DoublyLinkedList* const list, DoublyLinkedList_Entry* const entry)
{
	if (list->head == NULL)
		list->tail = entry;
	else
		list->head->previous = entry;

	entry->next = list->head;
	entry->previous = NULL;

	list->head = entry;
}

void DoublyLinkedList_PushBack(DoublyLinkedList* const list, DoublyLinkedList_Entry* const entry)
{
	if (list->tail == NULL)
		list->head = entry;
	else
		list->tail->next = entry;

	entry->next = NULL;
	entry->previous = list->tail;

	list->tail = entry;
}

void DoublyLinkedList_Remove(DoublyLinkedList* const list, DoublyLinkedList_Entry* const entry)
{
	if (entry->previous == NULL)
		list->head = entry->next;
	else
		entry->previous->next = entry->next;

	if (entry->next == NULL)
		list->tail = entry->previous;
	else
		entry->next->previous = entry->previous;
}
