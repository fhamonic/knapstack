/*
Boost Software License - Version 1.0 - August 17th, 2003

Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license (the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third-parties to whom the Software is furnished to
do so, all subject to the following:

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer,
must be included in all copies of the Software, in whole or in part, and
all derivative works of the Software, unless such copies or derivative
works are solely in the form of machine-executable object code generated by
a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.*/
#ifndef KNAPSTACK_ALL_HPP
#define KNAPSTACK_ALL_HPP

#ifndef KNAPSTACK_BRANCH_AND_BOUND_HPP
#define KNAPSTACK_BRANCH_AND_BOUND_HPP

#include <numeric>
#include <stack>
#include <vector>

#include <range/v3/view/zip.hpp>
#include <range/v3/algorithm/sort.hpp>
#include <range/v3/algorithm/remove_if.hpp>

#ifndef KNAPSTACK_INSTANCE_HPP
#define KNAPSTACK_INSTANCE_HPP

#include <limits>
#include <vector>

namespace Knapstack {
    template <typename Value, typename Cost>
    class Instance {
    public:
        class Item {
        public:
            Value value;
            Cost cost;
        public:
            Item(Value v, Cost c)
                : value{v}
                , cost{c}
                {}
            Item(const Item & item)
                : value{item.value}
                , cost{item.cost}
                {}
            double getRatio() const { 
                if(cost == 0) return std::numeric_limits<double>::max();
                return static_cast<double>(value) / static_cast<double>(cost);
            }
            bool operator<(const Item& other) const { return getRatio() > other.getRatio(); }
        };
    private:
        Cost budget;
        std::vector<Item> items;
    public:
        Instance() {}

        void setBudget(Cost b) { budget = b; }
        Cost getBudget() const { return budget; }

        void addItem(Value v, Cost w) { items.push_back(Item(v, w)); }
        size_t itemCount() const { return items.size(); }

        const std::vector<Item> & getItems() const { return items; }
        const Item getItem(int i) const { return items[i]; }
        const Item operator[](int i) const { return getItem(i); }
    };
} //namespace Knapstack

#endif //KNAPSTACK_INSTANCE_HPP
#ifndef KNAPSTACK_SOLUTION_HPP
#define KNAPSTACK_SOLUTION_HPP

#include <vector>

namespace Knapstack {
    template <template<typename,typename> class TInstance, typename Value, typename Cost>
    class Solution {
    private:
        const TInstance<Value,Cost> & instance;
        std::vector<bool> _taken;
    public:
        Solution(const TInstance<Value,Cost> & i) : instance(i), _taken(i.itemCount()) {}

        void add(size_t i) { _taken[i] = true; }
        void set(size_t i, bool b) { _taken[i] = b; }
        void remove(size_t i) { _taken[i] = false; }
        bool isTaken(size_t i) { return _taken[i]; }

        auto & operator[](size_t i) { return _taken[i]; }

        Value getValue() const {
            Value sum{};
            for(size_t i=0; i<instance.itemCount(); ++i)
                if(_taken[i]) sum += instance[i].value;
            return sum;
        }
        Cost getCost() const { 
            Cost sum{};
            for(size_t i=0; i<instance.itemCount(); ++i)
                if(_taken[i]) sum += instance[i].cost;
            return sum;
        }
    };
} //namespace Knapstack

#endif //KNAPSTACK_SOLUTION_HPP

namespace Knapstack {
    template <template<typename,typename> class Inst, typename Value, typename Cost>
    class BranchAndBound {
    public:
        using TInstance = Inst<Value,Cost>;
        using TItem = typename TInstance::Item;
        using TSolution = Solution<Inst, Value, Cost>;
    private:
        double computeUpperBound(const std::vector<TItem> & sorted_items, size_t depth, Value bound_value, Cost bound_budget_left) { 
            for(; depth<sorted_items.size(); ++depth) {
                const TItem & item = sorted_items[depth];
                if(bound_budget_left <= item.cost)
                    return bound_value + bound_budget_left * item.getRatio();
                bound_budget_left -= item.cost;
                bound_value += item.value;
            }
            return bound_value;
        }

        std::stack<int> iterative_bnb(const std::vector<TItem> & sorted_items, Cost budget_left) {
            const int nb_items = sorted_items.size();
            int depth = 0;
            Value value = 0;
            Value best_value = 0;
            std::stack<int> stack;
            std::stack<int> best_stack;
            goto begin;
        backtrack:
            while(!stack.empty()) {
                depth = stack.top();
                stack.pop();
                value -= sorted_items[depth].value;
                budget_left += sorted_items[depth++].cost;
                for(; depth<nb_items; ++depth) {
                    if(budget_left < sorted_items[depth].cost) continue;
                    if(computeUpperBound(sorted_items, depth, value, budget_left) <= best_value)
                        goto backtrack;
                begin:
                    value += sorted_items[depth].value;
                    budget_left -= sorted_items[depth].cost;
                    stack.push(depth);
                }
                if(value <= best_value) 
                    continue;
                best_value = value;
                best_stack = stack;
            }
            return best_stack;
        }
    public:
        BranchAndBound() {}   
        
        TSolution solve(const TInstance & instance) {
            std::vector<TItem> sorted_items = instance.getItems();
            std::vector<int> permuted_id(instance.itemCount());
            std::iota(permuted_id.begin(), permuted_id.end(), 0);

            auto zip_view = ranges::view::zip(sorted_items, permuted_id);
            auto end = ranges::remove_if(zip_view, [&](const auto & r) { 
                return r.first.cost > instance.getBudget(); 
            });
            const ptrdiff_t new_size = std::distance(zip_view.begin(), end);
            sorted_items.erase(sorted_items.begin() + new_size, sorted_items.end());
            permuted_id.erase(permuted_id.begin() + new_size, permuted_id.end());
            ranges::sort(zip_view, [](auto p1, auto p2){ return p1.first < p2.first; });
            
            std::stack<int> best_stack = iterative_bnb(sorted_items, instance.getBudget());

            TSolution solution(instance);
            while(! best_stack.empty()) {
                solution.add(permuted_id[best_stack.top()]);
                best_stack.pop();
            }
            return solution;
        }
    };
} //namespace Knapstack

#endif //KNAPSTACK_BRANCH_AND_BOUND_HPP
#ifndef UBOUNDED_KNAPSTACK_BRANCH_AND_BOUND_HPP
#define UBOUNDED_KNAPSTACK_BRANCH_AND_BOUND_HPP

#include <numeric>
#include <stack>
#include <vector>

#include <range/v3/view/zip.hpp>
#include <range/v3/algorithm/sort.hpp>
#include <range/v3/algorithm/remove_if.hpp>

#ifndef UNBOUNDED_KNAPSTACK_INSTANCE_HPP
#define UNBOUNDED_KNAPSTACK_INSTANCE_HPP

#include <limits>
#include <vector>

namespace UnboundedKnapstack {
    template <typename Value, typename Cost>
    using Instance = Knapstack::Instance<Value, Cost>;
} //namespace UnboundedKnapstack

#endif //UNBOUNDED_KNAPSTACK_INSTANCE_HPP
#ifndef UNBOUNDED_KNAPSTACK_SOLUTION_HPP
#define UNBOUNDED_KNAPSTACK_SOLUTION_HPP

#include <vector>

namespace UnboundedKnapstack {
    template <template<typename,typename> class TInstance, typename Value, typename Cost>
    class Solution {
    private:
        const TInstance<Value,Cost> & instance;
        std::vector<int> _nb_taken;
    public:
        Solution(const TInstance<Value,Cost> & i) : instance(i), _nb_taken(i.itemCount()) {}

        void add(size_t i) { ++_nb_taken[i]; }
        void set(size_t i, int n) { _nb_taken[i] = n; }
        void remove(size_t i) { _nb_taken[i] = 0; }
        bool isTaken(size_t i) { return _nb_taken[i] > 0; }

        int & operator[](size_t i) { return _nb_taken[i]; }

        Value getValue() const {
            Value sum{};
            for(size_t i=0; i<instance.itemCount(); ++i)
                sum += _nb_taken[i] * instance[i].value;
            return sum;
        }
        Cost getCost() const { 
            Cost sum{};
            for(size_t i=0; i<instance.itemCount(); ++i)
                sum += _nb_taken[i] * instance[i].cost;
            return sum;
        }
    };
} //namespace UnboundedKnapstack

#endif //UNBOUNDED_KNAPSTACK_SOLUTION_HPP

namespace UnboundedKnapstack {
    template <template<typename,typename> class Inst, typename Value, typename Cost>
    class BranchAndBound {
    public:
        using TInstance = Inst<Value,Cost>;
        using TItem = typename TInstance::Item;
        using TSolution = Solution<Inst, Value, Cost>;
    private:
        double computeUpperBound(const std::vector<TItem> & sorted_items, size_t depth, Value bound_value, Cost bound_budget_left) { 
            for(; depth<sorted_items.size(); ++depth) {
                const TItem & item = sorted_items[depth];
                if(bound_budget_left <= item.cost)
                    return bound_value + bound_budget_left * item.getRatio();
                const int nb_take = bound_budget_left / item.cost;
                bound_budget_left -= nb_take * item.cost;
                bound_value += nb_take * item.value;
            }
            return bound_value;
        }

        std::stack<std::pair<int,int>> iterative_bnb(const std::vector<TItem> & sorted_items, Cost budget_left) {
            const int nb_items = sorted_items.size();
            int depth = 0;
            Value value = 0;
            Value best_value = 0;
            std::stack<std::pair<int,int>> stack;
            std::stack<std::pair<int,int>> best_stack;
            goto begin;
        backtrack:
            while(!stack.empty()) {
                depth = stack.top().first;
                if(--stack.top().second == 0)
                    stack.pop();
                value -= sorted_items[depth].value;
                budget_left += sorted_items[depth++].cost;
                for(; depth<nb_items; ++depth) {
                    if(budget_left < sorted_items[depth].cost) continue;
                    if(computeUpperBound(sorted_items, depth, value, budget_left) <= best_value)
                        goto backtrack;
                begin:
                    const int nb_take = budget_left / sorted_items[depth].cost;
                    value += nb_take * sorted_items[depth].value;
                    budget_left -= nb_take * sorted_items[depth].cost;
                    stack.emplace(depth, nb_take);
                }
                if(value <= best_value) 
                    continue;
                best_value = value;
                best_stack = stack;
            }
            return best_stack;
        }
    public:
        BranchAndBound() {}   
        
        TSolution solve(const TInstance & instance) {
            std::vector<TItem> sorted_items = instance.getItems();
            std::vector<int> permuted_id(instance.itemCount());
            std::iota(permuted_id.begin(), permuted_id.end(), 0);

            auto zip_view = ranges::view::zip(sorted_items, permuted_id);
            auto end = ranges::remove_if(zip_view, [&](const auto & r) { 
                return r.first.cost > instance.getBudget(); 
            });
            const ptrdiff_t new_size = std::distance(zip_view.begin(), end);
            sorted_items.erase(sorted_items.begin() + new_size, sorted_items.end());
            permuted_id.erase(permuted_id.begin() + new_size, permuted_id.end());
            ranges::sort(zip_view, [](auto p1, auto p2){ return p1.first < p2.first; });
            
            std::stack<std::pair<int,int>> best_stack = 
                    iterative_bnb(sorted_items, instance.getBudget());

            TSolution solution(instance);
            while(! best_stack.empty()) {
                solution.set(permuted_id[best_stack.top().first], 
                             best_stack.top().second);
                best_stack.pop();
            }
            return solution;
        }
    };
} //namespace UnboundedKnapstack

#endif //UBOUNDED_KNAPSTACK_BRANCH_AND_BOUND_HPP

#endif //KNAPSTACK_ALL_HPP