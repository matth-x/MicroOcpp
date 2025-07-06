// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_METERVALUE_H
#define MO_METERVALUE_H

#include <stddef.h>
#include <stdint.h>
#include <MicroOcpp/Model/Metering/ReadingContext.h>
#include <MicroOcpp/Version.h>

#if MO_ENABLE_V16 || MO_ENABLE_V201

#ifndef MO_SAMPLEDVALUE_FLOAT_FORMAT
#define MO_SAMPLEDVALUE_FLOAT_FORMAT "%.2f"
#endif

#ifndef MO_SAMPLEDVALUE_STRING_MAX_LEN
#define MO_SAMPLEDVALUE_STRING_MAX_LEN 1000 //maximum acceptable sampled value string length
#endif

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

#if MO_ENABLE_V201

/* Compound type for transmitting SignedMeterValues via OCPP. The firmware should generate signature
 * data and keep it ready until MO queries it using the `getSignedValue` callback. Then, the firmware
 * can fill this struct provided by MO. Probably the signature data needs to be duplicated into the
 * heap per malloc. The firmware can set the `onDestroy` callback to free the memory again. */
struct MO_SignedMeterValue201 {
    double value;

    /* Signed data. If not null, MO will dereference these strings and send them to the server. When
     * MO no longer needs these strings, it calls onDestroy (guaranteed execution). It's possible to
     * malloc string buffers for these fields and to free them again in onDestroy */
    const char *signedMeterData;
    const char *signingMethod;
    const char *encodingMethod;
    const char *publicKey;

    //set this as needed
    void* user_data; //MO will forward this pointer to onDestroy
    void (*onDestroy)(void *user_data); //MO calls it shortly before destruction of this struct
};

#endif //MO_ENABLE_V201

typedef enum {
    MO_MeterInputType_UNDEFINED = 0,
    MO_MeterInputType_Int,
    MO_MeterInputType_Float,
    MO_MeterInputType_String,
    MO_MeterInputType_IntWithArgs,
    MO_MeterInputType_FloatWithArgs,
    MO_MeterInputType_StringWithArgs,

    #if MO_ENABLE_V201
    MO_MeterInputType_SignedValueWithArgs,
    #endif //MO_ENABLE_V201
}   MO_MeterInputType;

typedef struct {

    void* user_data;
    union {
        int32_t (*getInt)();
        float (*getFloat)();
        //if `size` is large enough, writes string to `buf` and returns written string length (not
        //including terminating zero). If `size` is too short, returns string length which would have
        //been written if `buf` was large enough. MO may retry with a larger buf. Adds a terminating 0.
        //Note: `buf` will never be larger than (MO_SAMPLEDVALUE_STRING_MAX_LEN + 1)
        int (*getString)(char *buf, size_t size);

        int32_t (*getInt2)(MO_ReadingContext readingContext, unsigned int evseId, void *user_data);
        float (*getFloat2)(MO_ReadingContext readingContext, unsigned int evseId, void *user_data);
        int (*getString2)(char *buf, size_t size, MO_ReadingContext readingContext, unsigned int evseId, void *user_data); //see `getString()`

        #if MO_ENABLE_V201
        //if signed value exists, fills `val` with the signature data and returns true. If no signed
        //data exists, returns false.
        bool (*getSignedValue2)(MO_SignedMeterValue201* val, MO_ReadingContext readingContext, unsigned int evseId, void *user_data);
        #endif //MO_ENABLE_V201
    };

    uint8_t type; //MO_MeterInputType in dense representation. Use `mo_meterInput_setType()` to define
    uint8_t mo_flags; //flags which MO uses internally

    #if MO_ENABLE_V201
    int8_t unitOfMeasureMultiplier;
    #endif //MO_ENABLE_V201

    //OCPP-side attributes. Zero-copy strings
    const char *format;
    const char *measurand;
    const char *phase;
    const char *location;
    const char *unit;
} MO_MeterInput;

void mo_meterInput_init(MO_MeterInput *mInput, MO_MeterInputType type);
MO_MeterInputType mo_meterInput_getType(MO_MeterInput *mInput);
void mo_meterInput_setType(MO_MeterInput *mInput, MO_MeterInputType type);

#ifdef __cplusplus
} //extern "C"

#include <MicroOcpp/Core/Time.h>
#include <MicroOcpp/Model/Metering/ReadingContext.h>
#include <MicroOcpp/Core/Memory.h>

namespace MicroOcpp {

class Clock;

struct SampledValue : public MemoryManaged {
    MO_MeterInput& meterInput;

    union {
        int32_t valueInt;
        float valueFloat;
        char *valueString = nullptr;

        #if MO_ENABLE_V201
        MO_SignedMeterValue201 *valueSigned;
        #endif //MO_ENABLE_V201
    };

    SampledValue(MO_MeterInput& meterInput);
    ~SampledValue();

    enum class Type : uint8_t {
        UNDEFINED,
        Int,
        Float,
        String,
        SignedValue,
    };
    Type getType();
};

struct MeterValue : public MemoryManaged {
    Timestamp timestamp;
    Vector<SampledValue*> sampledValue;
    MO_ReadingContext readingContext = MO_ReadingContext_UNDEFINED; //all SampledValues in a MeterValue have the same MO_ReadingContext

    int txNr = -1;
    unsigned int opNr = 1;
    unsigned int attemptNr = 0;
    Timestamp attemptTime;

    MeterValue();
    ~MeterValue();

    bool parseJson(Clock& clock, Vector<MO_MeterInput>& meterInputs, JsonObject in); //only parses internal format
    int getJsonCapacity(int ocppVersion, bool internalFormat);
    bool toJson(Clock& clock, int ocppVersion, bool internalFormat, JsonObject out);
};

} //namespace MicroOcpp
#endif //MO_ENABLE_V16 || MO_ENABLE_V201
#endif //__cplusplus
#endif
