#pragma once

#include <unordered_map>
#include <unordered_set>

class FunctionTypeCallRecorderBase
{
public:
    virtual void Clear() = 0;
    virtual ~FunctionTypeCallRecorderBase() {}
};

template <typename R, typename... ARGS>
    class FunctionTypeCallRecorder : public FunctionTypeCallRecorderBase
{
    typedef R (*FunctionPointerType)(ARGS...);
    struct FunctionCall
    {
        // TODO: add argument values
        uint64_t callSequenceNumber;
    };
    
    typedef std::unordered_multimap<FunctionPointerType, FunctionCall> RecordedCallMapType;

    RecordedCallMapType recordedCalls;
    static FunctionTypeCallRecorder functionTypeRegister;

    void RecordFunctionCall(FunctionPointerType function, uint64_t sequenceNumber);
    bool DidCallFunction(FunctionPointerType function);
    
    virtual void Clear() override
    {
        this->recordedCalls.clear();
    }
    
    friend class MockCalls;
};

template <typename R, typename... ARGS>
    FunctionTypeCallRecorder<R, ARGS...> FunctionTypeCallRecorder<R, ARGS...>::functionTypeRegister;

template <typename R, typename... ARGS>
    void FunctionTypeCallRecorder<R, ARGS...>::RecordFunctionCall(FunctionPointerType function, uint64_t sequenceNumber)
{
    this->recordedCalls.insert(std::make_pair(function, FunctionCall { sequenceNumber }));
}

template <typename R, typename... ARGS>
    bool FunctionTypeCallRecorder<R, ARGS...>::DidCallFunction(FunctionPointerType function)
{
    typename RecordedCallMapType::const_iterator foundCall = this->recordedCalls.find(function);
    return foundCall != this->recordedCalls.end();
}

class MockCalls
{
    std::unordered_set<FunctionTypeCallRecorderBase*> functionTypeCallRecorders;
    uint64_t nextCallSequenceNumber = 0;
    
    static MockCalls singleton;
    
public:
    template <typename R, typename... ARGS>
        static void RecordFunctionCall(R (*fn)(ARGS...), ARGS... args)
    {
        singleton.functionTypeCallRecorders.insert(&FunctionTypeCallRecorder<R, ARGS...>::functionTypeRegister);
        
        FunctionTypeCallRecorder<R, ARGS...>::functionTypeRegister.RecordFunctionCall(fn, singleton.nextCallSequenceNumber++);
    }
    
    static void Clear();
    
    template <typename R, typename... ARGS>
        static bool DidCallFunction(R (*fn)(ARGS...))
    {
        return FunctionTypeCallRecorder<R, ARGS...>::functionTypeRegister.DidCallFunction(fn);
    }
};

