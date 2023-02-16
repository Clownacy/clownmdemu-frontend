#ifndef DOUBLY_LINKED_LIST_H
#define DOUBLY_LINKED_LIST_H

typedef struct DoublyLinkedList_Entry
{
	/* The previous pointer must be first because of a giant hack:
	   In the pool implementation, this is part of a union with 'SinglyLinkedList_Entry',
	   causing the first pointer to be overwritten with the other list's next pointer.
	   The object allocator uses a pool, and expects the doubly-linked list's next pointer
	   to be valid even if the object was just deallocated. */
	/* TODO: Think of a way to do this without a giant hack. */
	struct DoublyLinkedList_Entry *previous;
	struct DoublyLinkedList_Entry *next;
} DoublyLinkedList_Entry;

typedef struct DoublyLinkedList
{
	DoublyLinkedList_Entry *head;
	DoublyLinkedList_Entry *tail;
} DoublyLinkedList;

void DoublyLinkedList_Initialise(DoublyLinkedList* const list);
void DoublyLinkedList_PushFront(DoublyLinkedList* const list, DoublyLinkedList_Entry* const entry);
void DoublyLinkedList_PushBack(DoublyLinkedList* const list, DoublyLinkedList_Entry* const entry);
void DoublyLinkedList_Remove(DoublyLinkedList* const list, DoublyLinkedList_Entry* const entry);

#endif /* DOUBLY_LINKED_LIST_H */
