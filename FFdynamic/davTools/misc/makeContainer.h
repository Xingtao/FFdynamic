#pragma once

#include <vector>

template <typename T>
class MakeVector {
public:
    typedef MakeVector<T> TheType;
    TheType & operator<< (const T & val) {
        m_data.push_back(val);
        return *this;
    }
    operator std::vector<T>() const {
        return m_data;
    }

private:
    std::vector<T> m_data;
};
