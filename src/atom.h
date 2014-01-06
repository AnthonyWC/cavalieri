#ifndef ATOM_H
#define ATOM_H

#include <unordered_map>
#include <memory>
#include <cds/init.h>
#include <cds/gc/hp.h>


template <class T>
class atom {
public:
  atom(T* ptr) : atomic_ptr_(ptr) {};
  atom() : atomic_ptr_(new T()) {};

  void update(std::function<T(const T&)> update_fn) {
    update_(update_fn, {});
  }

  void update(const T& t, std::function<void(const T&, const T&)> success_fn) {
    update_([&](const T&) { return t; }, success_fn);
  }

  void update(
      std::function<T(const T&)> update_fn,
      std::function<void(const T&, const T&)> success_fn
  )
  {
    update_(update_fn, success_fn);
  }

  void update_(
      std::function<T(const T&)> update_fn,
      std::function<void(const T&, const T&)> success_fn
  ) {

    T* nptr = nullptr;
    T* optr = nullptr;

    cds::gc::HP::Guard new_guard;

    do {
      cds::gc::HP::Guard old_guard;
      old_guard.assign(atomic_ptr_.load());
      if (nptr != nullptr) {
        delete nptr;
      }
      optr = (T*)(old_guard.get_native());
      nptr = new T(update_fn(*optr));
      new_guard.assign(nptr);
    } while (!atomic_ptr_.compare_exchange_strong(optr, nptr));

    if (success_fn) {
      success_fn(*optr, *nptr);
    }

    cds::gc::HP::retire(optr, retire);
  }



  void safe_read(std::function<void(const T&)> fn) {
    cds::gc::HP::Guard guard;
    guard.assign(atomic_ptr_.load());
    fn(*(T*)(guard.get_native()));
  }

  std::atomic<T*> & atomic_ptr() {
    return atomic_ptr_;
  }

  static void retire(T *ptr) {
    delete ptr;
  }

  static void attach_thread() {
    cds::threading::Manager::attachThread();
  }

  static void detach_thread() {
    cds::threading::Manager::detachThread();
  }

 ~atom() {
   cds::gc::HP::retire(atomic_ptr_.load(), retire);
  }

 private:
  std::atomic<T*> atomic_ptr_;
};

template <class T>
std::shared_ptr<atom<T>> make_shared_atom() {
  std::shared_ptr<atom<T>> s(new atom<T>(new T()));
  return s;
}

template <class T>
std::shared_ptr<atom<T>> make_shared_atom(const T t) {
  std::shared_ptr<atom<T>> s(new atom<T>(new T(t)));
  return s;
}

template <class K, class V>
void map_on_sync_insert(
  std::shared_ptr<atom<std::unordered_map<K,V>>> & m,
  K k,
  std::function<V()> fn_value,
  std::function<void(V&)> fn_inserted
)
{
  std::unordered_map<K,V>* nptr = nullptr;
  std::unordered_map<K,V>* optr = nullptr;
  do {
    cds::gc::HP::Guard guard;
    guard.assign(m->atomic_ptr().load());
    if (nptr != nullptr) {
      delete nptr;
    }
    optr = (std::unordered_map<K,V>*)(guard.get_native());
    auto it = optr->find(k);
    if (it != optr->end()) {
      fn_inserted(it->second);
      return;
    }
    nptr = new std::unordered_map<K,V>(*optr);
    nptr->insert({k, fn_value()});
  } while (!m->atomic_ptr().compare_exchange_strong(optr, nptr));

  cds::gc::HP::Guard guard;
  guard.assign(m->atomic_ptr().load());
  nptr = (std::unordered_map<K,V>*)(guard.get_native());
  auto it = nptr->find(k);
  assert(it != nptr->end());
  fn_inserted(it->second);

  cds::gc::HP::retire(optr, atom<std::unordered_map<K,V>>::retire);
}

#endif