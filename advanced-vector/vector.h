#pragma once

#include <cassert>
#include <cstdlib>
#include <new>
#include <memory>
#include <utility>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory& rhs) = delete;

    RawMemory(RawMemory&& other) noexcept 
        : buffer_(other.buffer_), capacity_(other.capacity_) 
    {
        other.buffer_ = nullptr;
    }

    RawMemory& operator=(RawMemory&& rhs) noexcept {
        if (this != &rhs) {
            Swap(rhs);
        }
        return *this;
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};


template <typename T>
class Vector {
public:
    Vector() = default;

    explicit Vector(size_t size)
        : data_(size), size_(size)
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }

    Vector(const Vector& other)
        : data_(other.size_), size_(other.size_)
    {
        std::uninitialized_copy_n(other.data_.GetAddress(), size_, data_.GetAddress());
    }

    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                Vector rhs_copy(rhs);
                Swap(rhs_copy);
            } else {
                std::copy(rhs.data_.GetAddress(), rhs.data_ + std::min(rhs.size_, size_), data_.GetAddress());
                size_ > rhs.size_ ? std::destroy_n(data_ + rhs.size_, size_ - rhs.size_)
                                  : std::uninitialized_copy_n(rhs.data_ + size_, rhs.size_ - size_, data_ + size_);           
            }
            size_ = rhs.size_;
        }
        return *this;
    }

    Vector(Vector&& other) noexcept {
        Swap(other);
    }

    Vector& operator=(Vector&& rhs) noexcept {
        if (this != &rhs) {
            Swap(rhs);
        }
        return *this;
    }

    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }
    
    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

    void PushBack(const T& value) {
        EmplaceBack(value);
    }

    void PushBack(T&& value) {
        EmplaceBack(std::move(value));
    }
    
    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        return *Emplace(cend(), std::forward<Args>(args)...);
    } 

    void PopBack() noexcept {
        assert(size_ != 0);
        data_[size_ - 1].~T();
        --size_;
    }      
    
    void Resize(size_t new_size) {
        if (new_size < size_) {
            std::destroy_n(data_ + new_size, size_ - new_size);
        } else {
            Reserve(new_size);
            std::uninitialized_value_construct_n(data_ + size_, new_size - size_);
        }
        size_ = new_size;
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }
        RawMemory<T> new_data(new_capacity);
        InitializeDataUsingOtherData(data_.GetAddress(), new_data.GetAddress(), size_);
        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }      
    
    using iterator = T*;
    using const_iterator = const T*;
    
    iterator begin() noexcept {
        return data_.GetAddress();
    }

    iterator end() noexcept {
        return data_ + size_;
    }

    const_iterator cbegin() const noexcept {
        return data_.GetAddress();
    }

    const_iterator cend() const noexcept {
        return data_ + size_;
    }
    
    const_iterator begin() const noexcept {
        return cbegin();
    }

    const_iterator end() const noexcept {
        return cend();
    }
    
    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        assert(pos >= begin() && pos <= end());
        if (size_ == Capacity()) {
            return EmplaceWithRealloc(pos, std::forward<Args>(args)...);
        } else {
            return EmplaceWithoutRealloc(pos, std::forward<Args>(args)...);
        }
    }
    
    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, value);
    }
    
    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, std::move(value));
    }
    
    iterator Erase(const_iterator pos) {
        assert(pos >= begin() && pos < end());
        size_t pos_index = pos - cbegin();
        std::move(data_ + pos_index + 1, end(), data_ + pos_index);
        PopBack();
        return data_ + pos_index;
    }

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }
    
private:
    template <typename InputIt>
    void InitializeDataUsingOtherData(InputIt start_from, InputIt start_to, size_t count) {
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(start_from, count, start_to);
        } else {
            std::uninitialized_copy_n(start_from, count, start_to);
        }
    }

    template <typename... Args>
    iterator EmplaceWithRealloc(const_iterator pos, Args&&... args) {
        size_t pos_index = pos - cbegin();
        RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
        new (new_data + pos_index) T(std::forward<Args>(args)...);
        try { 
            InitializeDataUsingOtherData(data_.GetAddress(), new_data.GetAddress(), pos_index);
        } catch(...) { 
            new_data[pos_index].~T();
            throw;                    
        } 
        try {            
            InitializeDataUsingOtherData(data_ + pos_index, new_data + pos_index + 1, size_ - pos_index);
        } catch(...) {
            std::destroy_n(new_data.GetAddress(), pos_index + 1);
            throw;                  
        }        
        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
        ++size_;
        return data_ + pos_index;        
    }

    template <typename... Args>
    iterator EmplaceWithoutRealloc(const_iterator pos, Args&&... args) {
        size_t pos_index = pos - cbegin();
        if (pos == cend()) {
            new (data_ + size_) T(std::forward<Args>(args)...);
        } else {
            T temp(std::forward<Args>(args)...);
            new (data_ + size_) T(std::move(data_[size_ - 1]));
            std::move_backward(data_ + pos_index, data_ + (size_ - 1), data_ + size_);
            data_[pos_index] = std::move(temp);
        }
        ++size_;
        return data_ + pos_index;            
    }

    RawMemory<T> data_;
    size_t size_ = 0;
};
