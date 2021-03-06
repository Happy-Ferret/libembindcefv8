#pragma once

#include <cstring>
#include <type_traits>

#ifdef EMSCRIPTEN
    #include <emscripten.h>
    #include <emscripten/bind.h>
#else
    #include "include/cef_client.h"
    #include "include/cef_app.h"
    #include <functional>
    #include <tuple>
#endif

#ifdef EMSCRIPTEN
    #define EMBINDCEFV8_BINDINGS EMSCRIPTEN_BINDINGS
#else
    #define EMBINDCEFV8_BINDINGS(name)\
        static struct EmbindCefV8Initializer_##name {\
            EmbindCefV8Initializer_##name() {\
                embindcefv8::getInitializers()[std::string(#name)] = std::function<void()>(&EmbindCefV8Initializer_##name::init);\
            }\
            static void init();\
        } EmbindCefV8Initializer_##name##_instance;\
        void EmbindCefV8Initializer_##name::init()
#endif

namespace embindcefv8
{
    template<typename T>
    struct GetBaseClass
    {
        using value = void;
    };

    #ifdef CEF
        using Initializer = std::function<void()>;
        using Registerer = std::function<void(CefRefPtr<CefV8Value>&)>;
        using ResultFunction = std::function<void(CefRefPtr<CefV8Value>&, const CefV8ValueList&)>;
        using GetterFunction = std::function<void(CefRefPtr<CefV8Value>&, void*)>;
        using SetterFunction = std::function<void(void*, const CefRefPtr<CefV8Value>&)>;
        using MethodFunction = std::function<void(CefRefPtr<CefV8Value>&, void*, const CefV8ValueList& arguments)>;
        using ConstructorFunction = std::function<void(void *&, const CefV8ValueList& arguments)>;

        std::map<std::string, Initializer> & getInitializers();
        void onContextCreated(CefV8Context* context);
        void setBrowser(CefRefPtr<CefBrowser> browser);
        bool hasContext();
        CefRefPtr<CefV8Value> & getModuleObject();
        std::vector<Registerer> & getRegisterers();

        class FuncHandler : public CefV8Handler
        {
        public:
            FuncHandler(ResultFunction & _func) : CefV8Handler()
            {
                func = _func;
            }

            virtual bool Execute(const CefString& name, CefRefPtr<CefV8Value> object, const CefV8ValueList& arguments, CefRefPtr<CefV8Value>& retval, CefString& exception) override
            {
                func(retval, arguments);
                return true;
            }

            IMPLEMENT_REFCOUNTING(FuncHandler);
        private:
            ResultFunction
                func;
        };

        template<typename T>
        struct IsValueObject
        {
            static constexpr bool value = false;
        };

        struct UserData : public CefBase
        {
            UserData(void * _data)
                : data(_data)
            {
            }

            UserData(const void * _data)
                : data(const_cast<void*>(_data))
            {
            }

            ~UserData()
            {
            }

            void
                * data;

            IMPLEMENT_REFCOUNTING(UserData);
        };

        class MethodHandler : public CefV8Handler
        {
        public:
            MethodHandler(MethodFunction & _method) : CefV8Handler()
            {
                method = _method;
            }

            virtual bool Execute(const CefString& name, CefRefPtr<CefV8Value> object, const CefV8ValueList& arguments, CefRefPtr<CefV8Value>& retval, CefString& exception) override
            {
                method(retval, dynamic_cast<UserData*>(object->GetUserData().get())->data, arguments);
                return true;
            }

            IMPLEMENT_REFCOUNTING(FuncHandler);
        private:
            MethodFunction
                method;
        };

        template<typename T>
        class Class;

        template<typename T>
        class ClassAccessor;

        template<typename T>
        struct ValueCreator;

        template<>
        struct ValueCreator<float>
        {
            static void create(CefRefPtr<CefV8Value>& retval, const float value)
            {
                retval = CefV8Value::CreateDouble(value);
            }
        };

        template<>
        struct ValueCreator<int>
        {
            static void create(CefRefPtr<CefV8Value>& retval, const int value)
            {
                retval = CefV8Value::CreateInt(value);
            }
        };

        template<>
        struct ValueCreator<unsigned>
        {
            static void create(CefRefPtr<CefV8Value>& retval, const unsigned value)
            {
                retval = CefV8Value::CreateUInt(value);
            }
        };

        template<>
        struct ValueCreator<unsigned char>
        {
            static void create(CefRefPtr<CefV8Value>& retval, const unsigned char value)
            {
                retval = CefV8Value::CreateUInt(value);
            }
        };

        template<>
        struct ValueCreator<bool>
        {
            static void create(CefRefPtr<CefV8Value>& retval, const bool value)
            {
                retval = CefV8Value::CreateBool(value);
            }
        };

        template<>
        struct ValueCreator<std::string>
        {
            static void create(CefRefPtr<CefV8Value>& retval, const std::string & value)
            {
                retval = CefV8Value::CreateString(value);
            }
        };

        template<typename T, class Enable = void>
        struct ValueCreatorCaller
        {
            static void create(CefRefPtr<CefV8Value>& retval, const T & value)
            {
                using type = std::remove_const_t<std::remove_reference_t<T>>;
                ValueCreator<type>::create(retval, const_cast<type &>(value));
            }
        };

        template<typename T>
        struct ValueCreatorCaller<T, std::enable_if_t<std::is_pointer_v<T>>>
        {
            static void create(CefRefPtr<CefV8Value>& retval, const T & value)
            {
                if(value == nullptr)
                {
                    retval = CefV8Value::CreateUndefined();
                    return;
                }

                using type = std::remove_const_t<std::remove_reference_t<std::remove_pointer_t<T>>>;
                ValueCreator<type>::create(retval, const_cast<type &>(*value));
            }
        };

        template<class T>
        class ValueObject;

        template<typename T, class Enable = void>
        struct ValueConverter
        {
            using Type = std::remove_const_t<std::remove_reference_t<T>>;

            template<class Q = T>
            static
            std::enable_if_t<!IsValueObject<Type>::value, Q>
            get(CefV8Value & v)
            {
                return * reinterpret_cast<T *>(dynamic_cast<UserData*>(v.GetUserData().get())->data);
            }

            template<class Q = Type>
            static
            std::enable_if_t<IsValueObject<Type>::value, Q>
            get(CefV8Value & v)
            {
                T
                    result;

                for(auto& kv : ValueObject<T>::setters)
                {
                    kv.second((void*) &result, v.GetValue(kv.first));
                }

                return result;
            }
        };

        template<typename T>
        struct ValueConverter<T, std::enable_if_t<std::is_pointer_v<T>>>
        {
            static T get(CefV8Value & v)
            {
                using type = std::remove_pointer_t<T>;

                return reinterpret_cast<type *>(dynamic_cast<UserData*>(v.GetUserData().get())->data);
            }
        };

        template<typename T>
        struct ValueConverter<T, std::enable_if_t<std::is_reference_v<T>&& std::is_const_v<std::remove_reference_t<T>>>>
        {
            using Type = std::remove_const_t<std::remove_reference_t<T>>;

            template<class Q = T>
            static
            std::enable_if_t<!IsValueObject<Type>::value, Q>
            get(CefV8Value & v)
            {
                using Type2 = std::remove_reference_t<T>;
                return *reinterpret_cast<Type2 *>(dynamic_cast<UserData*>(v.GetUserData().get())->data);
            }

            template<class Q = Type>
            static
            std::enable_if_t<IsValueObject<Type>::value, Q>
            get(CefV8Value & v)
            {
                Type
                    result;

                for(auto& kv : ValueObject<Type>::setters)
                {
                    kv.second((void*) &result, v.GetValue(kv.first));
                }

                return result;
            }
        };

        template<typename T>
        struct ValueConverter<T, std::enable_if_t<std::is_reference_v<T>&& !std::is_const_v<std::remove_reference_t<T>>>>
        {
            using Type = std::remove_const_t<std::remove_reference_t<T>>;

            template<class Q = T>
            static
            std::enable_if_t<!IsValueObject<Type>::value, Q>
            get(CefV8Value & v)
            {
                using Type2 = std::remove_reference_t<T>;
                return *reinterpret_cast<Type2 *>(dynamic_cast<UserData*>(v.GetUserData().get())->data);
            }

            template<class Q = T>
            static
            std::enable_if_t<IsValueObject<Type>::value, Q>
            get(CefV8Value & v)
            {
                Type
                    & result = * new Type();

                for(auto& kv : ValueObject<Type>::setters)
                {
                    kv.second((void*) &result, v.GetValue(kv.first));
                }

                return result;
            }
        };

        template<>
        struct ValueConverter<int>
        {
            static int get(CefV8Value & v)
            {
                return v.GetIntValue();
            }
        };

        template<>
        struct ValueConverter<unsigned int>
        {
            static int get(CefV8Value & v)
            {
                return v.GetUIntValue();
            }
        };

        template<>
        struct ValueConverter<float>
        {
            static float get(CefV8Value & v)
            {
                return float(v.GetDoubleValue());
            }
        };

        template<>
        struct ValueConverter<bool>
        {
            static float get(CefV8Value & v)
            {
                return float(v.GetBoolValue());
            }
        };

        template<>
        struct ValueConverter<double>
        {
            static double get(CefV8Value & v)
            {
                return v.GetDoubleValue();
            }
        };

        template<>
        struct ValueConverter<const char *>
        {
            static const char * get(CefV8Value & v)
            {
                return v.GetStringValue().ToString().c_str();
            }
        };

        template<>
        struct ValueConverter<std::string>
        {
            static std::string get(CefV8Value & v)
            {
                return v.GetStringValue().ToString();
            }
        };

        template<typename Result, typename ... Args>
        struct FunctionInvoker
        {
            template<int N>
            using GetArgType = std::tuple_element_t<N, std::tuple<Args...>>;

            static void call(Result (*staticFunction)(Args...), CefRefPtr<CefV8Value>& retval, const CefV8ValueList& arguments)
            {
                  internalCall(staticFunction, std::index_sequence_for<Args...>{}, retval, arguments);
            }

         private:
            template<class Function, std::size_t... Is>
            static void internalCall(Function func, std::index_sequence<Is...>, CefRefPtr<CefV8Value>& retval, const CefV8ValueList& arguments)
            {
                const Result & r = (*func)((ValueConverter<GetArgType<Is>>::get(*arguments[Is])) ...);
                ValueCreatorCaller<Result>::create(retval, r);
            }
        };

        template<typename T, typename Result, typename ... Args>
        struct MethodInvoker
        {
            template<int N>
            using GetArgType = std::tuple_element_t<N, std::tuple<Args...>>;

            static void call(Result (T::*field)(Args...), void * object, CefRefPtr<CefV8Value>& retval, const CefV8ValueList& arguments)
            {
                internalCall(field, object, std::index_sequence_for<Args...>{}, retval, arguments);
            }

            static void call(Result (T::*field)(Args...) const, void * object, CefRefPtr<CefV8Value>& retval, const CefV8ValueList& arguments)
            {
                internalCall(field, object, std::index_sequence_for<Args...>{}, retval, arguments);
            }

            static void call(Result (T::*field)(Args...), void * object, const CefV8ValueList& arguments)
            {
                internalCall(field, object, std::index_sequence_for<Args...>{}, arguments);
            }

            static void call(Result (T::*field)(Args...) const, void * object, const CefV8ValueList& arguments)
            {
                internalCall(field, object, std::index_sequence_for<Args...>{}, arguments);
            }

        private:
            template<class Field, std::size_t... Is>
            static void internalCall(Field field, void * object, std::index_sequence<Is...>, const CefV8ValueList& arguments)
            {
                ((*(T *) object).*field)((ValueConverter<GetArgType<Is>>::get(*arguments[Is])) ...);
            }

            template<class Field, std::size_t... Is>
            static void internalCall(Field field, void * object, std::index_sequence<Is...>, CefRefPtr<CefV8Value>& retval, const CefV8ValueList& arguments)
            {
                const Result & r = ((*(T *) object).*field)((ValueConverter<GetArgType<Is>>::get(*arguments[Is])) ...);
                ValueCreatorCaller<Result>::create(retval, r);
            }
        };

        template<typename T, typename ... Args>
        struct ConstructorInvoker
        {
            template<int N>
            using GetArgType = typename std::tuple_element_t<N, std::tuple<Args...>>;

            static T * call(const CefV8ValueList& arguments)
            {
                return internalCall(std::index_sequence_for<Args...>{}, arguments);
            }

        private:
            template<std::size_t... Is>
            static T * internalCall(std::index_sequence<Is...>, const CefV8ValueList& arguments)
            {
                return new T((ValueConverter<GetArgType<Is>>::get(*arguments[Is])) ...);
            }
        };
    #endif

    template<class T>
    class ValueObject
    {
    public:
        ValueObject(const char *_name)
        {
            name = _name;

            #ifdef EMSCRIPTEN
                emVo = new emscripten::value_object<T>(_name);
            #endif
        }

        ~ValueObject()
        {
            #ifdef EMSCRIPTEN
                delete emVo;
            #else
                if(constructors.size())
                {
                    auto copied_name = name;

                    getRegisterers().push_back(
                        [copied_name](CefRefPtr<CefV8Value> & module_object)
                        {
                            ResultFunction fc = [](CefRefPtr<CefV8Value>& retval, const CefV8ValueList& arguments) {
                                T * new_object;
                                constructors[arguments.size()]((void*&)new_object, arguments);
                                ValueCreator<T>::create(retval, * new_object);
                            };

                            CefRefPtr<CefV8Value> constructor_func = CefV8Value::CreateFunction(copied_name.c_str(), new FuncHandler(fc));
                            module_object->SetValue(copied_name.c_str(), constructor_func, V8_PROPERTY_ATTRIBUTE_NONE);
                        }
                    );
                }
            #endif
        }

        template<typename ... Args>
        ValueObject & constructor()
        {
            #ifdef EMSCRIPTEN
                T ( * func )(Args && ... args) =
                    [](Args && ... args) -> T
                    {
                        return T(std::forward<Args&&>( args )...);
                    };

                emscripten::function(name.c_str(), func);
            #else
                constructors[sizeof...(Args)] = [](void * & object, const CefV8ValueList& arguments) {
                    T * new_object = ConstructorInvoker<T, Args...>::call(arguments);
                    object = new_object;
                };
            #endif

            return *this;
        }

        template<class F>
        ValueObject & property(const char *name, F (T::*field))
        {
            #ifdef EMSCRIPTEN
                emVo->field(name, field);
            #else
                getters[name] = [field](CefRefPtr<CefV8Value>& retval, void * object) {
                    ValueCreatorCaller<F>::create(retval, (*(T *)object).*field);
                };
                setters[name] = [field](void * object, const CefRefPtr<CefV8Value>& cef_value) {
                    (*(T *)object).*field = ValueConverter<F>::get(*cef_value);
                };
            #endif

            return *this;
        }

        template<typename C>
        friend class ValueCreator;

        template<typename C, class>
        friend class ValueConverter;

    private:
        static std::string
            name;
        #ifdef EMSCRIPTEN
            emscripten::value_object<T>
                * emVo;
        #else
            static std::map<std::string, GetterFunction>
                getters;
            static std::map<std::string, SetterFunction>
                setters;
            static std::map<int, ConstructorFunction>
                constructors;
        #endif
    };

    template<class T>
    class Class
    {
    public:

        #ifdef EMSCRIPTEN
        using EmscriptenBaseClass = std::conditional_t<std::is_void_v<typename GetBaseClass<T>::value>, emscripten::internal::NoBaseClass, emscripten::base<typename GetBaseClass<T>::value>>;
        #endif

        Class() = delete;

        Class(const char *_name)
        {
            name = _name;

            #ifdef EMSCRIPTEN
                emClass = new emscripten::class_<T, EmscriptenBaseClass>(_name);
            #endif
        }

        ~Class()
        {
            #ifdef EMSCRIPTEN
                if(emClass)
                    delete emClass;
            #else
                auto copied_name = name;

                getRegisterers().push_back(
                    [copied_name](CefRefPtr<CefV8Value> & module_object)
                    {
                        ResultFunction fc = [](CefRefPtr<CefV8Value>& retval, const CefV8ValueList& arguments) {
                            T * new_object;
                            constructors[arguments.size()]((void*&)new_object, arguments);
                            ValueCreator<T>::create(retval, * new_object);
                        };

                        CefRefPtr<CefV8Value> constructor_func = CefV8Value::CreateFunction(copied_name.c_str(), new FuncHandler(fc));

                        for(auto& kv : staticFunctions)
                        {
                            constructor_func->SetValue(kv.first, kv.second, V8_PROPERTY_ATTRIBUTE_NONE);
                        }

                        module_object->SetValue(copied_name.c_str(), constructor_func, V8_PROPERTY_ATTRIBUTE_NONE);
                    }
                );
            #endif
        }

        template<typename ... Args>
        Class & constructor()
        {
            #ifdef EMSCRIPTEN
                emClass->template constructor<Args...>();
            #else
                constructors[sizeof...(Args)] = [](void * & object, const CefV8ValueList& arguments) {
                    T * new_object = ConstructorInvoker<T, Args...>::call(arguments);
                    object = new_object;
                };
            #endif

            return *this;
        }

        template<class F>
        Class & property(const char *name, F (T::*field))
        {
            #ifdef EMSCRIPTEN
                emClass->property(name, field);
            #else
                getters[name] = [field](CefRefPtr<CefV8Value>& retval, void * object) {
                    ValueCreatorCaller<F>::create(retval, (*(T *)object).*field);
                };
            #endif

            return *this;
        }

        template<typename Result, typename ... Args>
        Class & method(const char *name, Result (T::*field)(Args...))
        {
            #ifdef EMSCRIPTEN
                emClass->function(name, field, emscripten::allow_raw_pointers());
            #else
                MethodFunction m = [field](CefRefPtr<CefV8Value>& retval, void * object, const CefV8ValueList& arguments) {
                    MethodInvoker<T, Result, Args...>::call(field, object, retval, arguments);
                };

                methods[name] = CefV8Value::CreateFunction(name, new MethodHandler(m));
            #endif

            return *this;
        }

        template<typename Result, typename ... Args>
        Class & method(const char *name, Result (T::*field)(Args...) const)
        {
            #ifdef EMSCRIPTEN
                emClass->function(name, field, emscripten::allow_raw_pointers());
            #else
                MethodFunction m = [field](CefRefPtr<CefV8Value>& retval, void * object, const CefV8ValueList& arguments) {
                    MethodInvoker<T, Result, Args...>::call(field, object, retval, arguments);
                };

                methods[name] = CefV8Value::CreateFunction(name, new MethodHandler(m));
            #endif

            return *this;
        }

        template<typename ... Args>
        Class & method(const char *name, void (T::*field)(Args...))
        {
            #ifdef EMSCRIPTEN
                emClass->function(name, field, emscripten::allow_raw_pointers());
            #else
                MethodFunction m = [field](CefRefPtr<CefV8Value>& retval, void * object, const CefV8ValueList& arguments) {
                    MethodInvoker<T, void, Args...>::call(field, object, arguments);
                };

                methods[name] = CefV8Value::CreateFunction(name, new MethodHandler(m));
            #endif

            return *this;
        }

        template<typename ... Args>
        Class & method(const char *name, void (T::*field)(Args...) const)
        {
            #ifdef EMSCRIPTEN
                emClass->function(name, field, emscripten::allow_raw_pointers());
            #else
                MethodFunction m = [field](CefRefPtr<CefV8Value>& retval, void * object, const CefV8ValueList& arguments) {
                    MethodInvoker<T, void, Args...>::call(field, object, arguments);
                };

                methods[name] = CefV8Value::CreateFunction(name, new MethodHandler(m));
            #endif

            return *this;
        }

        template<typename Result, typename ... Args>
        Class & static_function(const char *name, Result (*staticFunction)(Args...))
        {
            #ifdef EMSCRIPTEN
                emClass->class_function(name, staticFunction, emscripten::allow_raw_pointers());
            #else
                ResultFunction m = [staticFunction](CefRefPtr<CefV8Value>& retval, const CefV8ValueList& arguments) {
                    FunctionInvoker<Result, Args...>::call(staticFunction, retval, arguments);
                };

                staticFunctions[name] = CefV8Value::CreateFunction(name, new FuncHandler(m));
            #endif

            return *this;
        }

        template<typename C>
        friend class ValueCreator;
        template<typename C>
        friend class ClassAccessor;

    //private:
        static std::string
            name;
        #ifdef EMSCRIPTEN
            emscripten::class_<T, EmscriptenBaseClass>
                * emClass;
        #else
            static std::map<std::string, GetterFunction>
                getters;
            static std::map<int, ConstructorFunction>
                constructors;
            static std::map<std::string, CefRefPtr<CefV8Value>>
                staticFunctions;
            static std::map<std::string, CefRefPtr<CefV8Value>>
                methods;
            static CefRefPtr<ClassAccessor<T>>
                classAccessor;
        #endif
    };

    template<class T>
    std::string ValueObject<T>::name;

    template<class T>
    std::string Class<T>::name;

    #ifdef CEF
        template<class T>
        std::map<std::string, GetterFunction> ValueObject<T>::getters;
        template<class T>
        std::map<std::string, SetterFunction> ValueObject<T>::setters;
        template<class T>
        std::map<int, ConstructorFunction> ValueObject<T>::constructors;

        template<class T>
        std::map<std::string, GetterFunction> Class<T>::getters;
        template<class T>
        std::map<int, ConstructorFunction> Class<T>::constructors;
        template<class T>
        std::map<std::string, CefRefPtr<CefV8Value>> Class<T>::staticFunctions;
        template<class T>
        std::map<std::string, CefRefPtr<CefV8Value>> Class<T>::methods;

        template<typename T>
        class ClassAccessor : public CefV8Accessor
        {
        public:
            ClassAccessor() : CefV8Accessor()
            {
            }

            virtual bool Get(const CefString& name, const CefRefPtr<CefV8Value> object, CefRefPtr<CefV8Value>& retval, CefString& exception) override
            {
                return get(name, object, retval, exception);
            }

            static bool get(const CefString& name, const CefRefPtr<CefV8Value> object, CefRefPtr<CefV8Value>& retval, CefString& exception)
            {
                return internalGet(name, object, retval, exception);
            }

            template<class Q = T>
            static
            std::enable_if_t<std::is_void_v<typename GetBaseClass<Q>::value>, bool>
            internalGet(const CefString& name, const CefRefPtr<CefV8Value> object, CefRefPtr<CefV8Value>& retval, CefString& exception)
            {
                {
                    auto it = Class<T>::getters.find(name);

                    if(it != Class<T>::getters.end())
                    {
                        it->second(retval, dynamic_cast<UserData*>(object->GetUserData().get())->data);
                        return true;
                    }
                }

                {
                    auto it = Class<T>::methods.find(name);

                    if(it != Class<T>::methods.end())
                    {
                        retval = it->second;
                        return true;
                    }
                }

                return false;
            }

            template<class Q = T>
            static
            std::enable_if_t<!std::is_void_v<typename GetBaseClass<Q>::value>, bool>
            internalGet(const CefString& name, const CefRefPtr<CefV8Value> object, CefRefPtr<CefV8Value>& retval, CefString& exception)
            {
                {
                    auto it = Class<T>::getters.find(name);

                    if(it != Class<T>::getters.end())
                    {
                        it->second(retval, dynamic_cast<UserData*>(object->GetUserData().get())->data);
                        return true;
                    }
                }

                {
                    auto it = Class<T>::methods.find(name);

                    if(it != Class<T>::methods.end())
                    {
                        retval = it->second;
                        return true;
                    }
                }

                using baseType = typename GetBaseClass<Q>::value;

                return ClassAccessor<baseType>::get(name, object, retval, exception);
            }

            virtual bool Set(const CefString& name, const CefRefPtr<CefV8Value> object, const CefRefPtr<CefV8Value> value, CefString& exception) override
            {
                return false;
            }

            IMPLEMENT_REFCOUNTING(ClassAccessor);
        };

        template<class T>
        CefRefPtr<ClassAccessor<T>> Class<T>::classAccessor = new ClassAccessor<T>();

        template<typename T>
        struct ValueCreator
        {
            template<class Q = T>
            static
            std::enable_if_t<IsValueObject<Q>::value, void>
            create(CefRefPtr<CefV8Value>& retval, const T& value)
            {
                retval = CefV8Value::CreateObject(nullptr);

                for(auto& kv : ValueObject<T>::getters)
                {
                    CefRefPtr<CefV8Value> field_value;
                    kv.second(field_value, (void*) &value);
                    retval->SetValue(kv.first, field_value, V8_PROPERTY_ATTRIBUTE_NONE);
                }
            }

            template<class Q = T>
            static
            std::enable_if_t<!IsValueObject<Q>::value, void>
            create(CefRefPtr<CefV8Value>& retval, const T& value)
            {
                retval = CefV8Value::CreateObject(&*Class<T>::classAccessor);

                retval->SetUserData(new UserData(& value));

                setGettersAndMethods(retval, value);
            }

            template<class Q = T>
            static
            std::enable_if_t<std::is_void_v<typename GetBaseClass<Q>::value>, void>
            setGettersAndMethods(CefRefPtr<CefV8Value>& retval, const T& value)
            {
                for(auto& kv : Class<T>::getters)
                {
                    retval->SetValue(kv.first, V8_ACCESS_CONTROL_DEFAULT, V8_PROPERTY_ATTRIBUTE_NONE);
                }

                for(auto& kv : Class<T>::methods)
                {
                    retval->SetValue(kv.first, V8_ACCESS_CONTROL_DEFAULT, V8_PROPERTY_ATTRIBUTE_NONE);
                }
            }

            template<class Q = T>
            static
            std::enable_if_t<!std::is_void_v<typename GetBaseClass<Q>::value>, void>
            setGettersAndMethods(CefRefPtr<CefV8Value>& retval, const T& value)
            {
                for(auto& kv : Class<T>::getters)
                {
                    retval->SetValue(kv.first, V8_ACCESS_CONTROL_DEFAULT, V8_PROPERTY_ATTRIBUTE_NONE);
                }

                for(auto& kv : Class<T>::methods)
                {
                    retval->SetValue(kv.first, V8_ACCESS_CONTROL_DEFAULT, V8_PROPERTY_ATTRIBUTE_NONE);
                }

                using baseType = typename GetBaseClass<Q>::value;

                ValueCreator<baseType>::setGettersAndMethods(retval, value);
            }
        };
    #endif

    void executeJavaScript(const char *str);

    template<typename T>
    void addGlobalObject(const T & object, const char *name)
    {
        #ifdef CEF
            std::string copied_name = name;
            const T * copied_object = & object;

            if(hasContext())
            {
                CefRefPtr<CefV8Value> new_value;
                ValueCreator<T>::create(new_value, * copied_object);
                getModuleObject()->SetValue(copied_name, new_value, V8_PROPERTY_ATTRIBUTE_NONE);
            }
            else
            {
                getRegisterers().push_back(
                    [copied_name, copied_object](CefRefPtr<CefV8Value> & module_object)
                    {
                        CefRefPtr<CefV8Value> new_value;

                        ValueCreator<T>::create(new_value, * copied_object);

                        module_object->SetValue(copied_name, new_value, V8_PROPERTY_ATTRIBUTE_NONE);
                    }
                    );
            }
        #else
            emscripten::val::global( "Module" ).set( name, emscripten::val( object ) );
        #endif
    }
}

#ifdef EMSCRIPTEN
    #define EMBINDCEFV8_DECLARE_CLASS(Class, Base) \
        namespace emscripten {\
            namespace internal {\
                template<>\
                struct BindingType<const Class> {\
                };\
                template<>\
                struct BindingType<Class&> {\
                    typedef Class* WireType;\
                    static WireType toWireType(Class& v) {\
                        return (WireType)&v;\
                    }\
                    static Class & fromWireType(WireType wt) {\
                        return *((Class *)wt);\
                    }\
                };\
                template<>\
                struct BindingType<Class*>{\
                    typedef Class* WireType;\
                    static WireType toWireType(Class* v) {\
                        return (WireType)v;\
                    }\
                    static Class* fromWireType(WireType wt) {\
                        return ((Class *)wt);\
                    }\
                };\
                template<>\
                struct BindingType<const Class&> {\
                    typedef const Class* WireType;\
                    static WireType toWireType(const Class& v) {\
                        return (WireType)&v;\
                    }\
                    static const Class & fromWireType(WireType wt) {\
                        return *((const Class *)wt);\
                    }\
                };\
                template<>\
                struct BindingType<Class>{\
                    typedef int WireType;\
                    static WireType toWireType(const Class& v) = delete;\
                    static Class fromWireType(WireType wt) = delete;\
                };\
            }\
        }\
        namespace embindcefv8\
        {\
        template<>\
        struct GetBaseClass<Class>\
        {\
            using value = Base;\
        };\
        }

    #define EMBINDCEFV8_DECLARE_VALUE_OBJECT(...)

    #define EMBINDCEFV8_DECLARE_ENUM(Enum)\
        EMSCRIPTEN_BINDINGS(Enum##_emscripten_binding) \
        { \
            emscripten::internal::_embind_register_integer(emscripten::internal::TypeID<Enum>::get(), #Enum, sizeof(int), std::numeric_limits<int>::min(), std::numeric_limits<int>::max());\
        }

    #define EMBINDCEFV8_DECLARE_STRING(Class, convert) \
        namespace emscripten {\
            namespace internal {\
                template<>\
                struct BindingType<Class> {\
                    typedef struct {\
                        size_t length;\
                        char data[1];\
                    } * WireType;\
                    static WireType toWireType(const Class& v) {\
                        auto length = strlen(v.convert());\
                        WireType wt = (WireType)malloc(sizeof(size_t) + length);\
                        wt->length = length;\
                        memcpy(wt->data, v.convert(), length);\
                        return wt;\
                    }\
                    static Class fromWireType(WireType v) {\
                        return Class(v->data, v->length);\
                    }\
                };\
            }\
        }

    #define EMBINDCEFV8_IMPLEMENT_STRING(Class) \
        EMSCRIPTEN_BINDINGS(Class) {\
            emscripten::internal::_embind_register_std_string(emscripten::internal::TypeID<Class>::get(), #Class);\
        }
#else
    #define EMBINDCEFV8_DECLARE_CLASS(Class, Base) \
        namespace embindcefv8\
        {\
        template<>\
        struct GetBaseClass<Class>\
        {\
            using value = Base;\
        };\
        }

    #define EMBINDCEFV8_DECLARE_VALUE_OBJECT(Class)\
        namespace embindcefv8\
        {\
        template<>\
        struct IsValueObject<Class>\
        {\
            static constexpr bool value = true;\
        };\
        }

    #define EMBINDCEFV8_DECLARE_ENUM(Enum)\
        namespace embindcefv8\
        {\
        template<> struct ValueCreator<Enum>\
        {\
            static void create(CefRefPtr<CefV8Value>& retval, const Enum value)\
            {\
                retval = CefV8Value::CreateInt((int)value);\
            }\
        };\
        template<> struct ValueConverter<Enum>\
        {\
            static Enum get(CefV8Value & v)\
            {\
                return (Enum)v.GetIntValue();\
            }\
        };\
        }

    #define EMBINDCEFV8_DECLARE_STRING(Class, convert)\
        namespace embindcefv8\
        {\
        template<> struct ValueCreator<Class>\
        {\
            static void create(CefRefPtr<CefV8Value>& retval, const Class value)\
            {\
                retval = CefV8Value::CreateString(value . convert ());\
            }\
        };\
        template<> struct ValueConverter<Class>\
        {\
            static Class get(CefV8Value & v)\
            {\
                return v.GetStringValue().ToString().c_str();\
            }\
        };\
        template<> struct ValueConverter<const Class &>\
        {\
            static Class get(CefV8Value & v)\
            {\
                return v.GetStringValue().ToString().c_str();\
            }\
        };\
        }

    #define EMBINDCEFV8_IMPLEMENT_STRING(Class)
#endif
