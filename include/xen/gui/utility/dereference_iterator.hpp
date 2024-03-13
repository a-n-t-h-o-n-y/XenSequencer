#pragma once

#include <utility>

namespace xen::gui::utility
{
/**
 * An iterator that dereferences its elements in operator*.
 *
 * Works for containers of pointers or other dereferencable types.
 *
 * @tparam Container The container type.
 */
template <typename Container>
class DereferenceIterator
{
  public:
    using iterator_type = typename Container::iterator;

    using difference_type = typename iterator_type::difference_type;
    using value_type = typename std::remove_reference<
        decltype(**std::declval<Container>().begin())>::type;
    using pointer = value_type *;
    using reference = value_type &;
    using iterator_category = typename iterator_type::iterator_category;

  public:
    explicit DereferenceIterator(iterator_type it) : it_{it}
    {
    }

  public:
    [[nodiscard]] auto operator*() const -> reference
    {
        return **it_;
    }

    [[nodiscard]] auto operator->() const -> pointer
    {
        return it_->get();
    }

    auto operator++() -> DereferenceIterator &
    {
        ++it_;
        return *this;
    }

    [[nodiscard]] auto operator++(int) -> DereferenceIterator
    {
        auto temp = *this;
        ++it_;
        return temp;
    }

    [[nodiscard]] auto operator==(DereferenceIterator const &other) const -> bool
    {
        return it_ == other.it_;
    }

    [[nodiscard]] auto operator!=(DereferenceIterator const &other) const -> bool
    {
        return it_ != other.it_;
    }

  private:
    iterator_type it_;
};

/**
 * A const iterator that dereferences its elements in operator*.
 *
 * Works for containers of pointers or other dereferencable types.
 *
 * @tparam Container The container type.
 */
template <typename Container>
class DereferenceConstIterator
{
  public:
    using const_iterator_type = typename Container::const_iterator;

    using value_type = typename std::remove_reference<
        decltype(**std::declval<Container>().begin())>::type;
    using pointer = const value_type *;
    using reference = const value_type &;
    using iterator_category = typename const_iterator_type::iterator_category;

  public:
    DereferenceConstIterator(const_iterator_type it) : it_{it}
    {
    }

  public:
    [[nodiscard]] auto operator*() const -> reference
    {
        return **it_;
    }

    [[nodiscard]] auto operator->() const -> pointer
    {
        return it_->get();
    }

    auto operator++() -> DereferenceConstIterator &
    {
        ++it_;
        return *this;
    }

    [[nodiscard]] auto operator++(int) -> DereferenceConstIterator
    {
        auto temp = *this;
        ++it_;
        return temp;
    }

    [[nodiscard]] auto operator==(const DereferenceConstIterator &other) const -> bool
    {
        return it_ == other.it_;
    }

    [[nodiscard]] auto operator!=(const DereferenceConstIterator &other) const -> bool
    {
        return it_ != other.it_;
    }

  private:
    const_iterator_type it_;
};

} // namespace xen::gui::utility