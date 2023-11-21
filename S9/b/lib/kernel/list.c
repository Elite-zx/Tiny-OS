#include "list.h"
#include "interrupt.h"

/**
 * plist_init - Initializes a new list.
 * @plist: Pointer to the list to initialize.
 *
 * Sets up the head and tail elements of the list.
 */
void list_init(struct list *plist) {
  plist->head.prev = NULL;
  plist->head.next = &plist->tail;
  plist->tail.next = NULL;
  plist->tail.prev = &plist->head;
}

/**
 * list_insert_before - Inserts an element before the specified position.
 * @posn: Position before which to insert.
 * @elem: Element to insert.
 *
 * Inserts 'elem' before 'posn' in the list. Interrupts are disabled during
 * the operation to ensure atomicity.
 */
void list_insert_before(struct list_elem *posn, struct list_elem *elem) {
  /* Turn off interrupts to ensure atomic operations  */
  enum intr_status old_status = intr_disable();

  elem->next = posn;
  elem->prev = posn->prev;
  posn->prev->next = elem;
  posn->prev = elem;

  intr_set_status(old_status);
}

/**
 * list_push - Pushes an element to the front of the list.
 * @plist: List to which the element is added.
 * @elem: Element to add.
 *
 * Adds 'elem' at the beginning of 'plist'.
 */
void list_push(struct list *plist, struct list_elem *elem) {
  list_insert_before(plist->head.next, elem);
}

/**
 * list_append - Appends an element to the end of the list.
 * @plist: List to which the element is added.
 * @elem: Element to append.
 *
 * Adds 'elem' at the end of 'plist'.
 */
void list_append(struct list *plist, struct list_elem *elem) {
  list_insert_before(&plist->tail, elem);
}

/**
 * list_remove - Removes an element from the list.
 * @elem: Element to remove.
 *
 * Disconnects 'elem' from its neighboring elements. Interrupts are disabled
 * during the operation to ensure atomicity.
 */
void list_remove(struct list_elem *elem) {
  enum intr_status old_status = intr_disable();

  elem->prev->next = elem->next;
  elem->next->prev = elem->prev;

  intr_set_status(old_status);
}

/**
 * list_pop - Pops the first element from the list.
 * @plist: List from which to pop the element.
 *
 * Removes and returns the first element from 'plist'.
 */
struct list_elem *list_pop(struct list *plist) {
  struct list_elem *elem = plist->head.next;
  list_remove(elem);
  return elem;
}

/**
 * list_elem_find - Checks if an element is in the list.
 * @plist: List to search in.
 * @obj_elem: Element to search for.
 *
 * Returns 'true' if 'obj_elem' is found in 'plist', 'false' otherwise.
 */
bool list_elem_find(struct list *plist, struct list_elem *obj_elem) {
  struct list_elem *iter = plist->head.next;
  while (iter != &plist->tail) {
    if (iter == obj_elem)
      return true;
    iter = iter->next;
  }
  return false;
}

/**
 * list_empty - Checks if the list is empty.
 * @plist: List to check.
 *
 * Returns 'true' if 'plist' is empty, 'false' otherwise.
 */
bool list_empty(struct list *plist) { return plist->head.next == &plist->tail; }

/**
 * list_traversal - Traverses the list and applies a function.
 * @plist: List to traverse.
 * @func: Function to apply.
 * @arg: Argument to pass to the function.
 *
 * Applies 'func' to each element of 'plist' until it returns 'true'.
 * Returns the element for which 'func' returned 'true' or 'NULL' if none.
 */
struct list_elem *list_traversal(struct list *plist, function func, int arg) {
  if (list_empty(plist))
    return NULL;

  struct list_elem *iter = plist->head.next;
  while (iter != &plist->tail) {
    if (func(iter, arg))
      return iter;
    iter = iter->next;
  }
  return NULL;
}

/**
 * list_len - Calculates the length of the list.
 * @plist: List to calculate the length of.
 *
 * Returns the number of elements in 'plist'.
 */
uint32_t list_len(struct list *plist) {
  uint32_t len = 0;
  struct list_elem *iter = plist->head.next;
  while (iter != &plist->tail) {
    ++len;
    iter = iter->next;
  }
  return len;
}
