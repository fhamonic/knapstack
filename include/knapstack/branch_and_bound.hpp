#ifndef KNAPSTACK_BRANCH_AND_BOUND_HPP
#define KNAPSTACK_BRANCH_AND_BOUND_HPP

#include <numeric>
#include <stack>
#include <vector>

#include <range/v3/view/zip.hpp>
#include <range/v3/algorithm/sort.hpp>
#include <range/v3/algorithm/remove_if.hpp>

#include "knapstack/instance.hpp"
#include "knapstack/solution.hpp"

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