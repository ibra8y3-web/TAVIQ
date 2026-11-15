#pragma once
#include <cstddef>
#include <functional>
namespace taviq {
void parallelFor(std::size_t begin,std::size_t end,std::size_t grain,const std::function<void(std::size_t)>& fn,std::size_t workers=0);
}