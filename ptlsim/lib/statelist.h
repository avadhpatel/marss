#ifndef STATELIST_H
#define STATELIST_H
#include <globals.h>
#include <superstl.h>

/*
 * Iterate through a linked list of objects where each object directly inherits
 * only from the selfqueuelink class or otherwise has a selfqueuelink object
 * as the first member.
 *
 * This iterator supports mutable lists, meaning the current entry (obj) may
 * be safely removed from the list and/or moved to some other list without
 * affecting the next object processed.
 *
 * This does NOT mean you can remove any object from the list other than the
 * current object obj - to do this, copy the list of pointers to an array and
 * then process that instead.
 * copy from ooocore.h
 */

#define foreach_list_mutable_linktype(L, obj, entry, nextentry, linktype) \
  linktype* entry; \
  linktype* nextentry; \
  for (entry = (L).next, nextentry = entry->next, prefetch(entry->next), obj = (typeof(obj))entry; \
    entry != &(L); entry = nextentry, nextentry = entry->next, prefetch(nextentry), obj = (typeof(obj))entry)

#define foreach_list_mutable(L, obj, entry, nextentry) foreach_list_mutable_linktype(L, obj, entry, nextentry, selfqueuelink)

#define foreach_list_mutable_linktype_backwards(L, obj, entry, preventry, linktype) \
  linktype* entry; \
  linktype* preventry; \
  for (entry = (L).prev, preventry = entry->prev, prefetch(entry->prev), obj = (typeof(obj))entry; \
    entry != &(L); entry = preventry, preventry = entry->prev, prefetch(preventry), obj = (typeof(obj))entry)

#define foreach_list_mutable_backwards(L, obj, entry, preventry) foreach_list_mutable_linktype_backwards(L, obj, entry, preventry, selfqueuelink)

  struct StateList;

  struct ListOfStateLists: public array<StateList*, 64> {
    int count;

    ListOfStateLists() { count = 0; }

    int add(StateList* list);
    void reset();
  };

  struct StateList: public selfqueuelink {
    char* name;
    int count;
    int listid;
    W64 dispatch_source_counter;
    W64 issue_source_counter;
    W32 flags;

    StateList() { name = NULL; listid = 0; reset(); }

    ~StateList() { if(name) delete name; }

    StateList(const char* name_, W32 flags_ = 0): flags(flags_){ name = strdup(name_); listid = 0; reset();}

    void init(const char* name, ListOfStateLists& lol, W32 flags = 0);

    StateList(const char* name, ListOfStateLists& lol, W32 flags = 0);

    // simulated asymmetric c++ array constructor:
    StateList& operator ()(const char* name, ListOfStateLists& lol, W32 flags = 0) {
      init(name, lol, flags);
      return *this;
    }

    void reset();

    selfqueuelink* dequeue() {
      if (empty())
        return NULL;
      count--;
      assert(count >=0);
      selfqueuelink* obj = removehead();
      return obj;
    }

    selfqueuelink* enqueue(selfqueuelink* entry) {
      entry->addtail(this);
      count++;
      return entry;
    }

    selfqueuelink* enqueue_after(selfqueuelink* entry, selfqueuelink* preventry) {
      if (preventry) entry->addhead(preventry);
	  else entry->addhead(this);
      count++;
      return entry;
    }

    selfqueuelink* remove(selfqueuelink* entry) {
      assert(entry->linked());
      entry->unlink();
      count--;
      assert(count >=0);
      return entry;
    }

    selfqueuelink* peek() {
      return (empty()) ? NULL : head();
    }

    StateList* remove_to_list(StateList* newqueue, bool place_at_head, selfqueuelink* entry) {
      remove(entry);
      if (place_at_head)
        enqueue_after(entry, newqueue->head());
      else
        newqueue->enqueue(entry);
      return newqueue;
    }

    void checkvalid();

    ostream& print(ostream& os) const{
      os << " (", count, " entries):";

      selfqueuelink* obj;
      foreach_list_mutable(*this, obj, entry, nextentry) {
        obj->print(os);
        os << endl;
      }
      os << endl;
      return os;
    }

    ostream& print(ostream& os, const W64& tag) const{
      os << " tag: ", (void*)tag, " ";
      print(os);
      return os;
    }
  };

  static inline ostream& operator <<(ostream& os, const StateList& list) {
    return list.print(os);
  }

struct FixStateListObject : public selfqueuelink
{
	int idx;
	bool free;

	ostream& print(ostream& os) const {
		os << "idx[", idx, "]";
		return os;
	}
};

template<typename T, int SIZE>
struct FixStateList
{

	FixStateList() {
		size_ = SIZE;
		reset();
	}

	T* peek() {
		return (T*)usedList_.peek();
	}

	T* head() {
		return (T*)usedList_.peek();
	}

	T* tail() {
		return (T*)(usedList_.empty() ? NULL : usedList_.tail());
	}

	int count() const {
		return usedList_.count;
	}

    int remaining() const {
        return freeList_.count;
    }

	int size() const {
		return size_;
	}

	bool isFull() const {
		return (freeList_.count == 0);
	}

	T* alloc() {
		if unlikely (isFull()) return NULL;
		T* obj = (T*)freeList_.peek();
		obj->init();
		obj->free = false;
		freeList_.remove_to_list(&usedList_, false, (selfqueuelink*)obj );
		return obj;
	}

	void free(T* obj) {
		obj->free = true;
		usedList_.remove_to_list(&freeList_, false, (selfqueuelink*)obj);
	}

	bool empty() {
		return usedList_.empty();
	}

	void unlink(T* obj) {
		assert((selfqueuelink*)obj->linked());
		usedList_.remove((selfqueuelink*)obj);
	}

    /*
     * This is a very tricky function, first argument
     * is the entry we need to insert after the second argument
     * entry. If you want to add to the head of the list, pass the root
     * argument as NULL
     */
	void insert_after(T* entry, T* root) {
		assert((selfqueuelink*)entry->unlinked());
		if(root)
			assert((selfqueuelink*)root->linked());
		usedList_.enqueue_after((selfqueuelink*)entry,
				(selfqueuelink*)root);
	}

	void print_all(ostream& os) {
		os << "used list: ", endl;
		usedList_.print(os);
		os << "free list: ", endl;
		freeList_.print(os);
	}

	void print(ostream& os) const {
		usedList_.print(os);
	}

	void reset() {
		usedList_.reset();
		freeList_.reset();
		foreach(i, SIZE) {
			objects_[i].idx = i;
			objects_[i].free = true;
			freeList_.enqueue((selfqueuelink*)&(objects_[i]));
		}
	}

	StateList& list() {
		return usedList_;
	}

	T& operator[](size_t idx) {
		return objects_[idx];
	}

	private:
		array<T, SIZE> objects_;
		int size_;
		StateList freeList_;
		StateList usedList_;

};

template<typename T, int SIZE>
static inline ostream& operator <<(ostream& os, const FixStateList<T, SIZE>& list)
{
	list.print(os);
	return os;
}

#endif
