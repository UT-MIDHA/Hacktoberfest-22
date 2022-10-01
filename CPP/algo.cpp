
#include <functional>
#include <stdexcept>
#include <tuple>
#include <type_traits>

#include <deque>
#include <iostream>
#include <vector>

using std::begin;
using std::end;

template <class T>
using unq = typename std::remove_const<
            typename std::remove_reference<T>::type>::type;

template <class T>
class optional                                                //  {{{1
{
public:
  using  stored_t = unq<T>;
  struct nullopt_t {};
  static nullopt_t nullopt;
  class bad_optional_access : std::logic_error
  {
  public:
    bad_optional_access() : std::logic_error("no value") {}
  };
private:
  struct _empty_t {};
  union { _empty_t _empty; stored_t _value; };
  bool  _has_value = false;
private:
  template<class... Args>
  void _construct(Args&&... args)
  { ::new(std::addressof(_value)) stored_t(std::forward<Args...>(args...));
    _has_value = true; }
  void _destruct() { _has_value = false; _value.~stored_t(); }
public:
  optional()            : _empty{} {}
  optional(nullopt_t)   : optional() {}
  optional(const T& t)  : _value(t), _has_value(true) {}
  ~optional()             { reset(); }

  optional& operator = (nullopt_t)
  { reset(); return *this; }
  template<class U>
  optional& operator = (U&& value)
  {
    if (_has_value) _value = std::forward<U>(value);
    else            _construct(std::forward<U>(value));
    return *this;
  }

  template<class... Args>
  void emplace(Args&&... args)
  { reset(); _construct(std::forward<Args>(args)...); }
  template<class U, class... Args>
  void emplace(std::initializer_list<U> ilist, Args&&... args)
  { reset(); _construct(ilist, std::forward<Args>(args)...); }
  void reset()
  { if (_has_value) _destruct(); }

  T& value() &
  { if (!_has_value) throw bad_optional_access(); return _value; }
  const T& value() const &
  { if (!_has_value) throw bad_optional_access(); return _value; }
  T&& value() &&
  { if (!_has_value) throw bad_optional_access(); return std::move(_value); }
  const T&& value() const &&
  { if (!_has_value) throw bad_optional_access(); return std::move(_value); }

  T& operator*() &                { return _value; }
  const T& operator*() const &    { return _value; }
  T&& operator*() &&              { return std::move(_value); }
  const T&& operator*() const &&  { return std::move(_value); }

  template<class U>
  T value_or(U&& d) &&
  { return _has_value ? std::move(**this)
                      : static_cast<T>(std::forward<U>(d)); }
  template<class U>
  T value_or(U&& d) const &
  { return _has_value ? **this
                      : static_cast<T>(std::forward<U>(d)); }

  explicit operator bool() const  { return _has_value; }
  bool has_value()         const  { return _has_value; }
};                                                            //  }}}1

/* ... TODO ... */

template <class T, class ItA, class ItB>
class Chain                                                   //  {{{1
{
public:
  class iterator                                              //  {{{2
  {
  private:
    Chain c;
  public:
    iterator(Chain c) : c(c) {}
    bool not_at_end()
    {
      return c.it_a != c.end_a || c.it_b != c.end_b;
    }
    bool operator != (const iterator&)
    {
      return not_at_end();
    }
    void operator++()
    {
      if      (c.it_a != c.end_a) ++c.it_a;
      else if (c.it_b != c.end_b) ++c.it_b;
    }
    unq<T> operator*()
    {
      if      (c.it_a != c.end_a) return *c.it_a;
      else if (c.it_b != c.end_b) return *c.it_b;
      else throw std::out_of_range(
        "Chain::iterator::operator*(): end reached");
    }
  };                                                          //  }}}2
private:
  ItA it_a; const ItA end_a;
  ItB it_b; const ItB end_b;
public:
  Chain(const ItA& begin_a, const ItA& end_a,
        const ItB& begin_b, const ItB& end_b)
    : it_a(begin_a), end_a(end_a), it_b(begin_b), end_b(end_b) {}
  Chain(const Chain&) = default;
//Chain(Chain&& rhs)
//  : it_a(rhs.it_a), end_a(rhs.end_a),
//    it_b(rhs.it_b), end_b(rhs.end_b)
//{
//  std::cout << "*** Chain MOVE ***" << std::endl;           //  TODO
//}
  iterator begin() { return iterator(*this); }
  iterator end()   { return iterator(*this); }
};                                                            //  }}}1

template <class SeqA, class SeqB>
auto chain(SeqA&& seq_a, SeqB&& seq_b)                        //  {{{1
  -> Chain<decltype(*begin(seq_a)), decltype(begin(seq_a)),
                                    decltype(begin(seq_b))>
{
  return Chain<decltype(*begin(seq_a)), decltype(begin(seq_a)),
                                        decltype(begin(seq_b))>
    (begin(seq_a), end(seq_a), begin(seq_b), end(seq_b));
}                                                             //  }}}1

template <class F, class T, class It>
class Filter                                                  //  {{{1
{
public:
  class iterator                                              //  {{{2
  {
  private:
    Filter c; optional<const unq<T>> v; bool peeked;
  private:
    void peek()
    {
      if (!peeked) {
        peeked = true;
        while (c.it != c.end_) {
          v.emplace(*c.it); ++c.it; if (c.f(*v)) break;
        }
        if (!(c.it != c.end_)) v.reset();
      }
    }
  public:
    iterator(Filter c) : c(c), v(), peeked(false) {}
    bool not_at_end()
    {
      peek(); return v.has_value();
    }
    bool operator != (const iterator&)
    {
      return not_at_end();
    }
    void operator++()
    {
      if(not_at_end()) peeked = false;
    }
    unq<T> operator*()
    {
      if (not_at_end()) return *v;
      throw std::out_of_range(
        "Filter::iterator::operator*(): end reached");
    }
  };                                                          //  }}}2
private:
  const F f; It it; const It end_;
public:
  Filter(F f, const It& begin, const It& end_)
    : f(f), it(begin), end_(end_) {}
  Filter(const Filter&) = default;
//Filter(Filter&& rhs)
//  : f(rhs.f), it(rhs.it), end_(rhs.end_)
//{
//  std::cout << "*** Filter MOVE ***" << std::endl;          //  TODO
//}
  iterator begin() { return iterator(*this); }
  iterator end()   { return iterator(*this); }
};                                                            //  }}}1

template <class F, class Seq>
auto filter(F f, Seq&& seq)                                   //  {{{1
  -> Filter<F, decltype(*begin(seq)), decltype(begin(seq))>
{
  return Filter<F, decltype(*begin(seq)), decltype(begin(seq))>
    (f, begin(seq), end(seq));
}                                                             //  }}}1

template <class F, class T, class It>
class Map                                                     //  {{{1
{
public:
  class iterator                                              //  {{{2
  {
  private:
    Map c;
  public:
    iterator(Map c) : c(c) {}
    bool not_at_end()
    {
      return c.it != c.end_;
    }
    bool operator != (const iterator&)
    {
      return not_at_end();
    }
    void operator++()
    {
      if (not_at_end()) ++c.it;
    }
    unq<T> operator*()
    {
      if (not_at_end()) return c.f(*c.it);
      else throw std::out_of_range(
        "Map::iterator::operator*(): end reached");
    }
  };                                                          //  }}}2
private:
  const F f; It it; const It end_;
public:
  Map(F f, const It& begin, const It& end_)
    : f(f), it(begin), end_(end_) {}
  Map(const Map&) = default;
//Map(Map&& rhs)
//  : f(rhs.f), it(rhs.it), end_(rhs.end_)
//{
//  std::cout << "*** Map MOVE ***" << std::endl;             //  TODO
//}
  iterator begin() { return iterator(*this); }
  iterator end()   { return iterator(*this); }
};                                                            //  }}}1

template <class F, class Seq>
auto map(F f, Seq&& seq)                                      //  {{{1
  -> Map<F, decltype(f(*begin(seq))), decltype(begin(seq))>
{
  return Map<F, decltype(f(*begin(seq))), decltype(begin(seq))>
    (f, begin(seq), end(seq));
}                                                             //  }}}1

template <class T, class It>
class Slice                                                   //  {{{1
{
public:
  class iterator                                              //  {{{2
  {
  private:
    Slice c; long n;
  private:
    bool not_done()
    {
      return (c.stop == -1 || n < c.stop) && c.it != c.end_;
    }
    void fwd()
    {
      while (c.start > 0 && not_done()) { ++c.it; ++n; --c.start; }
    }
  public:
    iterator(Slice c) : c(c), n(0) {}
    bool not_at_end()
    {
      fwd(); return not_done();
    }
    bool operator != (const iterator&)
    {
      return not_at_end();
    }
    void operator++()
    {
      fwd();
      for (long i = 0; i < c.step && not_done(); ++i, ++c.it, ++n);
    }
    unq<T> operator*()
    {
      if (not_at_end()) return *c.it;
      else throw std::out_of_range(
        "Slice::iterator::operator*(): end reached");
    }
  };                                                          //  }}}2
private:
  It it; const It end_; size_t start; const long stop, step;
public:
  Slice(const It& begin, const It& end_, const size_t& start,
        const long& stop, const long& step)
    : it(begin), end_(end_), start(start), stop(stop), step(step)
  {
    if (step < 0)
      throw std::invalid_argument("Slice(): step < 0");
    if (stop < -1)
      throw std::invalid_argument("Slice(): stop < -1");
  }
  Slice(const Slice&) = default;
//Slice(Slice&& rhs)
//  : it(rhs.it), end_(rhs.end_),
//    start(rhs.start), stop(rhs.stop), step(rhs.step)
//{
//  std::cout << "*** Slice MOVE ***" << std::endl;           //  TODO
//}
  iterator begin() { return iterator(*this); }
  iterator end()   { return iterator(*this); }
};                                                            //  }}}1

// ???
template <class Seq>
auto slice(Seq&& seq, const size_t& start,                    //  {{{1
           const long& stop, const long& step = 1)
  -> Slice<unq<decltype(*begin(seq))>, decltype(begin(seq))>
{
  return Slice<unq<decltype(*begin(seq))>, decltype(begin(seq))>
    (begin(seq), end(seq), start, stop, step);
}                                                             //  }}}1

// ???
template <class Seq>
auto slice(Seq&& seq, const long& stop)                       //  {{{1
  -> Slice<unq<decltype(*begin(seq))>, decltype(begin(seq))>
{
  return Slice<unq<decltype(*begin(seq))>, decltype(begin(seq))>
    (begin(seq), end(seq), 0, stop, 1);
}                                                             //  }}}1

template <class F, class T, class It>
class TakeWhile                                               //  {{{1
{
public:
  class iterator                                              //  {{{2
  {
  private:
    TakeWhile c; optional<const unq<T>> v; bool peeked;
  private:
    void peek()
    {
      if (!peeked) {
        peeked = true;
        if (c.it != c.end_) {
          v.emplace(*c.it); ++c.it; if (c.f(*v)) return;
        }
        v.reset();
      }
    }
  public:
    iterator(TakeWhile c) : c(c), v(), peeked(false) {}
    bool not_at_end()
    {
      peek(); return v.has_value();
    }
    bool operator != (const iterator&)
    {
      return not_at_end();
    }
    void operator++()
    {
      if (not_at_end()) peeked = false;
    }
    unq<T> operator*()
    {
      if (not_at_end()) return *v;
      throw std::out_of_range(
        "TakeWhile::iterator::operator*(): end reached");
    }
  };                                                          //  }}}2
private:
  F f; It it; const It end_;
public:
  TakeWhile(F f, const It& begin, const It& end_)
    : f(f), it(begin), end_(end_) {}
  TakeWhile(const TakeWhile&) = default;
//TakeWhile(TakeWhile&& rhs)
//  : f(rhs.f), it(rhs.it), end_(rhs.end_)
//{
//  std::cout << "*** TakeWhile MOVE ***" << std::endl;       //  TODO
//}
  iterator begin() { return iterator(*this); }
  iterator end()   { return iterator(*this); }
};                                                            //  }}}1

template <class F, class Seq>
auto take_while(F f, Seq&& seq)                               //  {{{1
  -> TakeWhile<F, decltype(*begin(seq)), decltype(begin(seq))>
{
  return TakeWhile<F, decltype(*begin(seq)), decltype(begin(seq))>
    (f, begin(seq), end(seq));
}                                                             //  }}}1

template <class S, class T, class ItA, class ItB>
class Zip                                                     //  {{{1
{
public:
  class iterator                                              //  {{{2
  {
  private:
    Zip c;
  public:
    iterator(Zip c) : c(c) {}
    bool not_at_end()
    {
      return c.it_a != c.end_a && c.it_b != c.end_b;
    }
    bool operator != (const iterator&)
    {
      return not_at_end();
    }
    void operator++()
    {
      if (not_at_end()) { ++c.it_a; ++c.it_b; }
    }
    std::tuple<unq<S>, unq<T>> operator*()
    {
      if (not_at_end()) return std::make_tuple(*c.it_a, *c.it_b);
      else throw std::out_of_range(
        "Zip::iterator::operator*(): end reached");
    }
  };                                                          //  }}}2
private:
  ItA it_a; const ItA end_a;
  ItB it_b; const ItB end_b;
public:
  Zip(const ItA& begin_a, const ItA& end_a,
      const ItB& begin_b, const ItB& end_b)
    : it_a(begin_a), end_a(end_a), it_b(begin_b), end_b(end_b) {}
  Zip(const Zip&) = default;
//Zip(Zip&& rhs)
//  : it_a(rhs.it_a), end_a(rhs.end_a),
//    it_b(rhs.it_b), end_b(rhs.end_b)
//{
//  std::cout << "*** Zip MOVE ***" << std::endl;             //  TODO
//}
  iterator begin() { return iterator(*this); }
  iterator end()   { return iterator(*this); }
};                                                            //  }}}1

template <class SeqA, class SeqB>
auto zip(SeqA&& seq_a, SeqB&& seq_b)                          //  {{{1
  -> Zip<decltype(*begin(seq_a)), decltype(*begin(seq_b)),
         decltype( begin(seq_a)), decltype( begin(seq_b))>
{
  return Zip<decltype(*begin(seq_a)), decltype(*begin(seq_b)),
             decltype( begin(seq_a)), decltype( begin(seq_b))>
    (begin(seq_a), end(seq_a), begin(seq_b), end(seq_b));
}                                                             //  }}}1

class IndexError : public std::out_of_range
{
public:
  IndexError() : std::out_of_range("invalid index") {}
};

class StopIteration : public std::out_of_range
{
public:
  StopIteration() : std::out_of_range("end reached") {}
};

class _generator
{
protected:
  int _line;
public:
  _generator() : _line(0) {}
};
