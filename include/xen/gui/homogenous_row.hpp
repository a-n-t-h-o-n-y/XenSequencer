#pragma once

#include <cstddef>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "utility/dereference_iterator.hpp"

namespace xen::gui
{

/**
 * @brief A row of components of the same type.
 *
 * This container owns the child components.
 *
 * @tparam T The type of the child components.
 */
template <typename T>
class HomogenousRow : public juce::Component
{
    static_assert(std::is_base_of<juce::Component, T>::value);

  public:
    using iterator = utility::DereferenceIterator<std::vector<std::unique_ptr<T>>>;
    using const_iterator =
        utility::DereferenceConstIterator<std::vector<std::unique_ptr<T>>>;

    using value_type = typename iterator::value_type;

  public:
    /**
     * @brief Create an empty HomogenousRow.
     *
     * @param flex The FlexItem to use as the default for each child.
     */
    HomogenousRow(juce::FlexItem flex = juce::FlexItem{}.withFlex(1.f),
                  bool paint_borders = false)
        : flex_item_{std::move(flex)}, paint_borders_{paint_borders}
    {
    }

  public:
    [[nodiscard]] auto get_children() const -> const std::vector<std::unique_ptr<T>> &
    {
        return children_;
    }

    [[nodiscard]] auto get_children() -> std::vector<std::unique_ptr<T>> &
    {
        return children_;
    }

    /**
     * @brief Emplace a child component at the given index.
     *
     * @tparam Args The types of the arguments to forward to the child's constructor.
     *
     * @param at The index to insert the child at.
     * @param args The arguments to forward to the child's constructor.
     * @return A reference to the newly created child.
     *
     * @throws std::out_of_range if at is greater than the number of children.
     */
    template <typename Child_t = T, typename... Args>
    auto emplace(std::size_t at, Args &&...args) -> T &
    {
        static_assert(std::is_base_of<T, Child_t>::value);

        if (at > children_.size())
        {
            throw std::out_of_range{"HomogenousRow::emplace: index out of range"};
        }

        return this->insert(at, std::make_unique<Child_t>(std::forward<Args>(args)...));
    }

    /**
     * @brief Emplace a child component at the end of the row.
     *
     * @tparam Args The types of the arguments to forward to the child's constructor.
     *
     * @param args The arguments to forward to the child's constructor.
     * @return A reference to the newly created child.
     */
    template <typename Child_t = T, typename... Args>
    auto emplace_back(Args &&...args) -> T &
    {
        return this->emplace<Child_t>(children_.size(), std::forward<Args>(args)...);
    }

    /**
     * @brief Insert a child component at the given index.
     *
     * @param at The index to insert the child at.
     * @param child The child to insert.
     * @return A reference to the newly created child.
     *
     * @throws std::out_of_range if at is greater than the number of children.
     */
    auto insert(std::size_t at, std::unique_ptr<T> child) -> T &
    {
        if (at > children_.size())
        {
            throw std::out_of_range{"HomogenousRow::insert: index out of range"};
        }

        auto &child_ref = *child;
        children_.emplace(std::next(children_.begin(), (std::ptrdiff_t)at),
                          std::move(child));
        this->initialize_child(child_ref);
        return child_ref;
    }

    /**
     * @brief Insert a child component at the end of the row.
     *
     * @param child The child to insert.
     * @return A reference to the newly created child.
     */
    auto push_back(std::unique_ptr<T> child) -> T &
    {
        return this->insert(children_.size(), std::move(child));
    }

    /**
     * Replaces the value at the given index with the given value and returns the old
     * value from that index.
     *
     * This will initialize the new child to be visible and will uninitialize the old
     * child so it is no longer a part of this Component.
     *
     * @param at The index to replace.
     * @param cell The new value.
     * @return The old value.
     *
     * @throws std::out_of_range if at >= this->size().
     */
    auto exchange(std::size_t at, std::unique_ptr<T> cell) -> std::unique_ptr<T>
    {
        if (at >= this->size())
        {
            throw std::out_of_range{"HomogenousRow::exchange: index out of range"};
        }

        auto old = std::move(children_[at]);
        children_[at] = std::move(cell);
        this->initialize_child(*children_[at]);
        this->uninitialize_child(*old);
        return old;
    }

    auto add_borders(bool paint_borders) noexcept -> void
    {
        paint_borders_ = paint_borders;
        this->repaint();
    }

  public:
    /**
     * @brief Remove all children from the row.
     */
    void clear() noexcept
    {
        this->removeAllChildren();
        children_.clear();
        this->resized();
    }

    /**
     * @brief Remove the child at the given index.
     *
     * @param index The index of the child to remove.
     *
     * @throws std::out_of_range if index is greater than the number of children.
     */
    auto erase(std::size_t index)
    {
        if (index >= children_.size())
        {
            throw std::out_of_range{"HomogenousRow::erase: index out of range"};
        }

        auto &child = children_[index];
        this->uninitialize_child(*child);
        children_.erase(std::next(children_.begin(), index));
    }

    /**
     * @brief Return a reference to the child at the given index.
     *
     * @param index The index of the child to return.
     * @return A reference to the child at the given index.
     *
     * @throws std::out_of_range if index is greater than the number of children.
     */
    auto at(std::size_t index) -> T &
    {
        if (index >= children_.size())
        {
            throw std::out_of_range{"HomogenousRow::at: index out of range"};
        }

        return *(children_[index]);
    }

    /**
     * @brief Return a const reference to the child at the given index.
     *
     * @param index The index of the child to return.
     * @return A const reference to the child at the given index.
     *
     * @throws std::out_of_range if index is greater than the number of children.
     */
    auto at(std::size_t index) const -> const T &
    {
        if (index >= children_.size())
        {
            throw std::out_of_range{"HomogenousRow::at: index out of range"};
        }

        return *(children_[index]);
    }

  public:
    /**
     * @return The number of children in the row.
     */
    [[nodiscard]] auto size() const noexcept -> std::size_t
    {
        return children_.size();
    }

    /**
     * @return True if the row has no children.
     */
    [[nodiscard]] auto empty() const noexcept -> bool
    {
        return children_.empty();
    }

    [[nodiscard]] auto begin() -> iterator
    {
        return iterator{children_.begin()};
    }

    [[nodiscard]] auto begin() const -> const_iterator
    {
        return const_iterator(children_.begin());
    }

    [[nodiscard]] auto cbegin() const -> const_iterator
    {
        return const_iterator(children_.cbegin());
    }

    [[nodiscard]] auto end() -> iterator
    {
        return iterator{children_.end()};
    }

    [[nodiscard]] auto end() const -> const_iterator
    {
        return const_iterator(children_.end());
    }

    [[nodiscard]] auto cend() const -> const_iterator
    {
        return const_iterator(children_.cend());
    }

  protected:
    auto resized() -> void override
    {
        auto flex_box = juce::FlexBox{};

        flex_box.flexDirection = juce::FlexBox::Direction::row;
        flex_box.justifyContent = juce::FlexBox::JustifyContent::flexStart;

        // Add each child component to the FlexBox
        for (auto &child : children_)
        {
            auto item = flex_item_;
            item.associatedComponent = child.get();
            flex_box.items.add(std::move(item));
        }

        flex_box.performLayout(this->getLocalBounds());
    }

    void paintOverChildren(juce::Graphics &g) override
    {
        if (paint_borders_)
        {
            g.setColour(juce::Colours::white);

            for (auto i = std::size_t{1}; i < children_.size(); ++i)
            {
                auto x = children_[i]->getX() - 1;
                g.drawLine((float)x, 0, (float)x, (float)this->getHeight(), 1);
            }
        }
    }

  private:
    /**
     * @brief Initialize a child component for use in the row.
     */
    auto initialize_child(T &child) -> void
    {
        this->addAndMakeVisible(child);
        this->resized();
    }

    /**
     * @brief Uninitialize a child component for use in the row.
     */
    auto uninitialize_child(T &child) -> void
    {
        this->removeChildComponent(&child);
        this->resized();
    }

  private:
    std::vector<std::unique_ptr<T>> children_;
    juce::FlexItem flex_item_;
    bool paint_borders_;
};

} // namespace xen::gui