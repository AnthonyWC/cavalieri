#ifndef STREAMS_H
#define STREAMS_H

#include <list>
#include <functional>
#include <proto.pb.h>

typedef const Event& e_t;

#define PRED(EXP) [](e_t e) { return (EXP); }
#define CHILD(EXP) {EXP}

typedef std::function<void(e_t)> stream_t;
typedef std::function<bool(e_t)> predicate_t;
typedef std::list<stream_t> children_t;
typedef std::map<std::string, std::string> with_changes_t;

void call_rescue(e_t e, const children_t& children);

stream_t prn();
stream_t with(const with_changes_t& changes, const children_t& children);
stream_t where(const predicate_t& predicate, const children_t& children,
               const children_t& else_children={});

struct Streams {
  std::list<stream_t> streams;
  void add_stream(stream_t stream);
  void process_message(const Msg& message);
};

#endif
