#include "timer.h"

namespace skynet {

// 添加一个定时器结点
void add_node(timer* T, timer_node* node)
{
    uint32_t time = node->expire;
    uint32_t current_time = T->time;

    if ((time | TIME_NEAR_MASK) == (current_time | TIME_NEAR_MASK))
    {
        link(&T->near[time & TIME_NEAR_MASK], node);
    }
    else
    {
        int i = 0;
        uint32_t mask = TIME_NEAR << TIME_LEVEL_SHIFT;
        for (i = 0; i < 3; i++)
        {
            if ((time | (mask-1)) == (current_time | (mask-1)))
            {
                break;
            }
            mask <<= TIME_LEVEL_SHIFT;
        }

        link(&T->t[i][((time >> (TIME_NEAR_SHIFT + i * TIME_LEVEL_SHIFT)) & TIME_LEVEL_MASK)], node);
    }
}

timer_node* link_clear(link_list* list)
{
    timer_node* ret = list->head.next;
    list->head.next = nullptr;
    list->tail = &(list->head);

    return ret;
}

void link(link_list* list, timer_node* node)
{
    list->tail->next = node;
    list->tail = node;
    node->next = nullptr;
}

void move_list(timer* T, int level, int idx)
{
    timer_node* current = link_clear(&T->t[level][idx]);
    while (current != nullptr)
    {
        timer_node* temp = current->next;
        add_node(T, current);
        current = temp;
    }
}

}
