#include <statelist.h>

void StateList::init(const char* name, ListOfStateLists& lol, W32 flags) {
  this->name = strdup(name);
  this->flags = flags;
  listid = lol.add(this);
  reset();
}


StateList::StateList(const char* name, ListOfStateLists& lol, W32 flags) {
  init(name, lol, flags);
}


void StateList::reset() {
  selfqueuelink::reset();
  count = 0;
  dispatch_source_counter = 0;
  issue_source_counter = 0;
}

int ListOfStateLists::add(StateList* list) {
  assert(count < lengthof(data));
  data[count] = list;
  return count++;
}

void ListOfStateLists::reset() {
  foreach (i, count) {
    data[i]->reset();
  }
}
