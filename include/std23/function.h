#ifndef INCLUDE_STD23_FUNCTION
#define INCLUDE_STD23_FUNCTION

#include "__functional_base.h"

#include <cstdarg>
#include <memory>
#include <new>

namespace std23
{

template<class Sig> struct _opt_fn_sig;

template<class R, class... Args> struct _opt_fn_sig<R(Args...)>
{
    using function_type = R(Args...);
    static constexpr bool is_variadic = false;

    template<class... T>
    static constexpr bool is_invocable_using =
        std::is_invocable_r_v<R, T..., Args...>;
};

template<class R, class... Args> struct _opt_fn_sig<R(Args......)>
{
    using function_type = R(Args...);
    static constexpr bool is_variadic = true;

    template<class... T>
    static constexpr bool is_invocable_using =
        std::is_invocable_r_v<R, T..., Args..., va_list>;
};

template<class R, class... Args> struct _copyable_function
{
    struct lvalue_callable
    {
        virtual R operator()(Args...) const = 0;
        virtual ~lvalue_callable() = default;

        void copy_into(std::byte *storage) const { copy_into_(storage); }
        void move_into(std::byte *storage) noexcept { move_into_(storage); }

      protected:
        virtual void copy_into_(void *) const = 0;
        virtual void move_into_(void *) noexcept = 0;
    };

    struct empty_target_object : lvalue_callable
    {
        [[noreturn]] R operator()(Args...) const override
        {
            throw std::bad_function_call{};
        }

        void copy_into_(void *location) const override
        {
            ::new (location) empty_target_object;
        }

        void move_into_(void *location) noexcept override
        {
            ::new (location) empty_target_object;
        }
    };

    template<class T> class target_object : lvalue_callable
    {
        std::conditional_t<std::is_pointer_v<T>, T, std::unique_ptr<T>> p_;

      public:
        R operator()(Args... args) const override
        {
            return std23::invoke_r<R>(*p_, std::forward<Args>(args)...);
        }

        template<class F>
        explicit target_object(F &&f) requires(
            _is_not_self<F, target_object> and not std::is_pointer_v<T>)
            : p_(std::make_unique<T>(std::forward<F>(f)))
        {}

        explicit target_object(T p) noexcept requires std::is_pointer_v<T>
            : p_(p)
        {}

        void copy_into_(void *location) const override
        {
            if constexpr (std::is_pointer_v<T>)
                ::new (location) auto(*this);
            else
                ::new (location) target_object(*p_);
        }

        void move_into_(void *location) noexcept override
        {
            ::new (location) auto(std::move(*this));
        }
    };

    template<class T> class target_object<T &> : lvalue_callable
    {
        T &target_;

      public:
        R operator()(Args... args) const override
        {
            return std23::invoke_r<R>(target_, std::forward<Args>(args)...);
        }

        explicit target_object(T &target) noexcept : target_(target) {}

        void copy_into_(void *location) const override
        {
            ::new (location) auto(*this);
        }

        void move_into_(void *location) noexcept override
        {
            ::new (location) auto(*this);
        }
    };
};

template<class S, class = typename _opt_fn_sig<S>::function_type>
class function;

template<class S, class R, class... Args> class function<S, R(Args...)>
{
    using signature = _opt_fn_sig<S>;

    template<class T>
    static constexpr bool is_lvalue_invocable =
        signature::template is_invocable_using<T &>;

    using copyable_function =
        std::conditional_t<signature::is_variadic,
                           _copyable_function<R, _param_t<Args>..., va_list &>,
                           _copyable_function<R, _param_t<Args>...>>;

    using lvalue_callable = typename copyable_function::lvalue_callable;
    using empty_target_object = typename copyable_function::empty_target_object;

    struct typical_target_object : lvalue_callable
    {
        union
        {
            void (*fp)() = nullptr;
            void *p;
        };
    };

    template<class F>
    using target_object_for =
        typename copyable_function::template target_object<
            std::unwrap_ref_decay_t<F>>;

    template<class F>
    static bool constexpr is_nothrow_initializer =
        std::is_nothrow_constructible_v<target_object_for<F>, F>;

    template<class F, class FD = std::decay_t<F>>
    static bool constexpr is_viable_initializer =
        std::is_copy_constructible_v<FD> and std::is_constructible_v<FD, F>;

    alignas(typical_target_object)
        std::byte storage_[sizeof(typical_target_object)];

    auto storage_location() noexcept -> void * { return &storage_; }

    auto target() noexcept
    {
        return std::launder(reinterpret_cast<lvalue_callable *>(&storage_));
    }

    auto target() const noexcept
    {
        return std::launder(
            reinterpret_cast<lvalue_callable const *>(&storage_));
    }

  public:
    using result_type = R;

    function() noexcept { ::new (storage_location()) empty_target_object; }
    function(std::nullptr_t) noexcept : function() {}

    template<class F>
    function(F &&f) noexcept(is_nothrow_initializer<F>) requires
        _is_not_self<F, function> and is_lvalue_invocable<F> and
        is_viable_initializer<F>
    {
        using T = target_object_for<F>;
        static_assert(sizeof(T) <= sizeof(storage_));

        if constexpr (_looks_nullable_to<F, function>)
        {
            if (f == nullptr)
            {
                ::new ((void *)this) function(nullptr);
                return;
            }
        }

        ::new (storage_location()) T(std::forward<F>(f));
    }

    function(function const &other) { other.target()->copy_into(storage_); }
    function(function &&other) noexcept { other.target()->move_into(storage_); }

    function &operator=(function const &other)
    {
        if (&other != this)
        {
            auto tmp = other;
            swap(tmp);
        }

        return *this;
    }

    function &operator=(function &&other) noexcept
    {
        if (&other != this)
        {
            this->~function();
            return *::new ((void *)this) auto(std::move(other));
        }
        else
            return *this;
    }

    void swap(function &other) noexcept { std::swap<function>(*this, other); }
    friend void swap(function &lhs, function &rhs) noexcept { lhs.swap(rhs); }

    ~function() { target()->~lvalue_callable(); }

    explicit operator bool() const noexcept
    {
        return dynamic_cast<empty_target_object const *>(target()) == nullptr;
    }

    friend bool operator==(function const &f, std::nullptr_t) noexcept
    {
        return !f;
    }

    R operator()(Args... args) const requires(!signature::is_variadic)
    {
        return (*target())(std::forward<Args>(args)...);
    }

#if defined(__GNUC__)
#if !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-value"
#else
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-value"
#endif
#endif

    R operator()(Args... args...) const
        requires(signature::is_variadic and sizeof...(Args) != 0)
    {
        struct raii
        {
            va_list data;
            ~raii() { va_end(data); }
        } va;
        va_start(va.data, (args, ...));
        return (*target())(std::forward<Args>(args)..., va.data);
    }

#if defined(__GNUC__)
#if !defined(__clang__)
#pragma GCC diagnostic pop
#else
#pragma clang diagnostic pop
#endif
#endif
};

// clang-format off

template<class F>
requires std::is_function_v<F>
function(F *) -> function<F>;

// clang-format on

template<class T>
function(T) -> function<_drop_first_arg_to_invoke_t<decltype(&T::operator())>>;

} // namespace std23

#endif
