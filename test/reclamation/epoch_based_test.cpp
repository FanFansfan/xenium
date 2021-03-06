#include <xenium/reclamation/epoch_based.hpp>

#include <gtest/gtest.h>

namespace {

using Reclaimer = xenium::reclamation::epoch_based<0>;

struct Foo : Reclaimer::enable_concurrent_ptr<Foo, 2>
{
  Foo** instance;
  Foo(Foo** instance) : instance(instance) {}
  virtual ~Foo() { if (instance) *instance = nullptr; }
};

template <typename T>
using concurrent_ptr = Reclaimer::concurrent_ptr<T>;
template <typename T> using marked_ptr = typename concurrent_ptr<T>::marked_ptr;

struct EpochBased : testing::Test
{
  Foo* foo = new Foo(&foo);
  marked_ptr<Foo> mp = marked_ptr<Foo>(foo, 3);

  void update_epoch()
  {
    // UpdateThreshold is set to 0, so we simply need create a guard_ptr to some dummy object
    // to trigger and epoch update.
    Foo dummy(nullptr);
    concurrent_ptr<Foo>::guard_ptr gp(&dummy);
  }

  void wrap_around_epochs()
  {
    update_epoch();
    update_epoch();
    update_epoch();
  }

  void TearDown() override
  {
    wrap_around_epochs();
    if (mp == nullptr)
      assert(foo == nullptr);
    else
      delete foo;
  }
};

TEST_F(EpochBased, mark_returns_the_same_mark_as_the_original_marked_ptr)
{
  concurrent_ptr<Foo>::guard_ptr gp(mp);
  EXPECT_EQ(mp.mark(), gp.mark());
}

TEST_F(EpochBased, get_returns_the_same_pointer_as_the_original_marked_ptr)
{
  concurrent_ptr<Foo>::guard_ptr gp(mp);
  EXPECT_EQ(mp.get(), gp.get());
}

TEST_F(EpochBased, reset_releases_ownership_and_sets_pointer_to_null)
{
  concurrent_ptr<Foo>::guard_ptr gp(mp);
  gp.reset();
  EXPECT_EQ(nullptr, gp.get());
}

TEST_F(EpochBased, reclaim_releases_ownership_and_the_object_gets_deleted_when_advancing_two_epochs)
{
  concurrent_ptr<Foo>::guard_ptr gp(mp);
  gp.reclaim();
  this->mp = nullptr;
  wrap_around_epochs();
  EXPECT_EQ(nullptr, foo);
  EXPECT_EQ(nullptr, gp.get());
}


struct WithCustomDeleter;
struct DummyDeleter {
  WithCustomDeleter* reference;
  void operator()(WithCustomDeleter* obj) const;
};
struct WithCustomDeleter : Reclaimer::enable_concurrent_ptr<WithCustomDeleter, 2, DummyDeleter> {};

void DummyDeleter::operator()(WithCustomDeleter* obj) const {
  EXPECT_EQ(reference, obj);
  delete obj;
}

TEST_F(EpochBased, supports_custom_deleters)
{
  concurrent_ptr<WithCustomDeleter>::guard_ptr gp(new WithCustomDeleter());
  gp.reclaim(DummyDeleter{gp.get()});
  wrap_around_epochs();
}

TEST_F(EpochBased, object_cannot_be_reclaimed_as_long_as_another_guard_protects_it)
{
  concurrent_ptr<Foo>::guard_ptr gp(mp);
  concurrent_ptr<Foo>::guard_ptr gp2(mp);
  gp.reclaim();
  wrap_around_epochs();
  EXPECT_NE(nullptr, foo);
}

TEST_F(EpochBased, copy_constructor_leads_to_shared_ownership_preventing_the_object_from_beeing_reclaimed)
{
  concurrent_ptr<Foo>::guard_ptr gp(mp);
  concurrent_ptr<Foo>::guard_ptr gp2(gp);
  gp.reclaim();
  this->mp = nullptr;
  wrap_around_epochs();
  EXPECT_NE(nullptr, foo);
}

TEST_F(EpochBased, move_constructor_moves_ownership_and_resets_source_object)
{
  concurrent_ptr<Foo>::guard_ptr gp(mp);
  concurrent_ptr<Foo>::guard_ptr gp2(std::move(gp));
  EXPECT_EQ(nullptr, gp.get());
  gp2.reclaim();
  this->mp = nullptr;
  wrap_around_epochs();
  EXPECT_EQ(nullptr, foo);
}

TEST_F(EpochBased, copy_assignment_leads_to_shared_ownership_preventing_the_object_from_beeing_reclaimed)
{
  concurrent_ptr<Foo>::guard_ptr gp(mp);
  concurrent_ptr<Foo>::guard_ptr gp2{};
  gp2 = gp;
  gp.reclaim();
  this->mp = nullptr;
  wrap_around_epochs();
  EXPECT_NE(nullptr, foo);
}

TEST_F(EpochBased, move_assignment_moves_ownership_and_resets_source_object)
{
  concurrent_ptr<Foo>::guard_ptr gp(mp);
  concurrent_ptr<Foo>::guard_ptr gp2{};
  gp2 = std::move(gp);
  gp2.reclaim();
  this->mp = nullptr;
  wrap_around_epochs();
  EXPECT_EQ(nullptr, gp.get());
  EXPECT_EQ(nullptr, foo);
}
}
