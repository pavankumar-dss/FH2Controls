// A controller of FH2Controls' own for Azimuth, without a driver.
//
// Azimuth gives the game the controllers Windows.Gaming.Input has: it asks once, at start, for
// RoGetActivationFactory("Windows.Gaming.Input.Gamepad", IGamepadStatics), registers for
// GamepadAdded/GamepadRemoved, lists get_Gamepads, and then only calls GetCurrentReading and
// put_Vibration on each pad (azimuth.exe 1.0.3, around +0x5D5FC0 and +0x5D6500). So the mod hands
// it the real statics wrapped: the same events, and a list with the real pads and one more, the
// keyboard's, whose readings come from the keys. Windows never sees a device: no driver, no
// "device connected" sound, no Guide button for the Game Bar.
#pragma once

#include <windows.h>

#include <inspectable.h>
#include <roapi.h>
#include <winstring.h>
#include <eventtoken.h>

#include <atomic>
#include <cstdint>

namespace wgi {

struct GamepadReading {
    uint64_t Timestamp;
    uint32_t Buttons;
    double LeftTrigger, RightTrigger;
    double LeftThumbstickX, LeftThumbstickY, RightThumbstickX, RightThumbstickY;
};
static_assert(sizeof(GamepadReading) == 0x40, "GamepadReading as Windows.Gaming.Input has it");

struct GamepadVibration {
    double LeftMotor, RightMotor, LeftTrigger, RightTrigger;
};

// {8BBCE529-D49C-39E9-9560-E47DDE96B7C8}
inline constexpr IID kIGamepadStatics = {0x8bbce529, 0xd49c, 0x39e9, {0x95, 0x60, 0xe4, 0x7d, 0xde, 0x96, 0xb7, 0xc8}};
// {BC7BB43C-0A69-3903-9E9D-A50F86A45DE5}
inline constexpr IID kIGamepad = {0xbc7bb43c, 0x0a69, 0x3903, {0x9e, 0x9d, 0xa5, 0x0f, 0x86, 0xa4, 0x5d, 0xe5}};
// {94EA2B94-E9CC-49E0-C0FF-EE64CA8F5B90}
inline constexpr IID kIAgileObject = {0x94ea2b94, 0xe9cc, 0x49e0, {0xc0, 0xff, 0xee, 0x64, 0xca, 0x8f, 0x5b, 0x90}};
inline constexpr wchar_t kGamepadClass[] = L"Windows.Gaming.Input.Gamepad";

// The interfaces, as their vtables are.
struct IGamepadStaticsRaw : IInspectable {
    virtual HRESULT STDMETHODCALLTYPE add_GamepadAdded(IUnknown* handler, EventRegistrationToken* token) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_GamepadAdded(EventRegistrationToken token) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_GamepadRemoved(IUnknown* handler, EventRegistrationToken* token) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_GamepadRemoved(EventRegistrationToken token) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Gamepads(IInspectable** view) = 0;
};

struct IVectorViewRaw : IInspectable {
    virtual HRESULT STDMETHODCALLTYPE GetAt(UINT32 index, IInspectable** item) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Size(UINT32* size) = 0;
    virtual HRESULT STDMETHODCALLTYPE IndexOf(IInspectable* item, UINT32* index, boolean* found) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetMany(UINT32 start, UINT32 capacity, IInspectable** items, UINT32* actual) = 0;
};

struct IGamepadRaw : IInspectable {
    virtual HRESULT STDMETHODCALLTYPE get_Vibration(GamepadVibration* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_Vibration(GamepadVibration value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetCurrentReading(GamepadReading* value) = 0;
};

using ReadFn = void (*)(GamepadReading* out);

// The free-threaded marshaler, aggregated, as Windows' own (agile) objects have it: handed to
// another apartment, the object is used as it is, not through a proxy COM would look for.
struct Ftm {
    IUnknown* inner = nullptr;
    void make(IUnknown* outer) { CoCreateFreeThreadedMarshaler(outer, &inner); }
    HRESULT query(REFIID iid, void** out) {
        if (!inner) {
            *out = nullptr;
            return E_NOINTERFACE;
        }
        return inner->QueryInterface(iid, out);
    }
    ~Ftm() {
        if (inner) inner->Release();
    }
};

inline HRESULT iids(ULONG* count, IID** out, const IID& iid) {
    *out = static_cast<IID*>(CoTaskMemAlloc(sizeof(IID)));
    if (!*out) return E_OUTOFMEMORY;
    **out  = iid;
    *count = 1;
    return S_OK;
}

inline HRESULT class_name(HSTRING* out) {
    return WindowsCreateString(kGamepadClass, static_cast<UINT32>(wcslen(kGamepadClass)), out);
}

// The keyboard's controller. There's one, and it lives as long as the game.
class KeyPad final : public IGamepadRaw {
public:
    explicit KeyPad(ReadFn read) : read_(read) { ftm_.make(static_cast<IGamepadRaw*>(this)); }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override {
        if (!out) return E_POINTER;
        if (iid == __uuidof(IMarshal)) return ftm_.query(iid, out);
        if (iid == __uuidof(IUnknown) || iid == __uuidof(IInspectable) || iid == kIAgileObject || iid == kIGamepad) {
            *out = static_cast<IGamepadRaw*>(this);
            AddRef();
            return S_OK;
        }
        *out = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs_; }
    ULONG STDMETHODCALLTYPE Release() override { return refs_ > 1 ? --refs_ : 1; } // never goes
    HRESULT STDMETHODCALLTYPE GetIids(ULONG* count, IID** out) override { return iids(count, out, kIGamepad); }
    HRESULT STDMETHODCALLTYPE GetRuntimeClassName(HSTRING* name) override { return class_name(name); }
    HRESULT STDMETHODCALLTYPE GetTrustLevel(TrustLevel* level) override {
        *level = BaseTrust;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE get_Vibration(GamepadVibration* value) override {
        if (!value) return E_POINTER;
        *value = vibration_;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE put_Vibration(GamepadVibration value) override {
        vibration_ = value; // a keyboard doesn't rumble
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetCurrentReading(GamepadReading* value) override {
        if (!value) return E_POINTER;
        *value = {};
        read_(value);
        reads_.fetch_add(1, std::memory_order_relaxed);
        return S_OK;
    }

    uint64_t reads() const { return reads_.load(); }

private:
    std::atomic<ULONG> refs_{1};
    Ftm ftm_;
    ReadFn read_;
    GamepadVibration vibration_{};
    std::atomic<uint64_t> reads_{0};
};

// get_Gamepads: the real pads, then the keyboard's.
class View final : public IVectorViewRaw {
public:
    View(IVectorViewRaw* real, KeyPad* pad) : real_(real), pad_(pad) { ftm_.make(static_cast<IVectorViewRaw*>(this)); }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override {
        if (!out) return E_POINTER;
        if (iid == __uuidof(IMarshal)) return ftm_.query(iid, out);
        if (iid == __uuidof(IUnknown) || iid == __uuidof(IInspectable) || iid == kIAgileObject) {
            *out = static_cast<IVectorViewRaw*>(this);
            AddRef();
            return S_OK;
        }
        *out = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs_; }
    ULONG STDMETHODCALLTYPE Release() override {
        const ULONG n = --refs_;
        if (n == 0) delete this;
        return n;
    }
    HRESULT STDMETHODCALLTYPE GetIids(ULONG* count, IID** out) override {
        *count = 0;
        *out   = nullptr;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetRuntimeClassName(HSTRING* name) override {
        static constexpr wchar_t kName[] =
            L"Windows.Foundation.Collections.IVectorView`1<Windows.Gaming.Input.Gamepad>";
        return WindowsCreateString(kName, static_cast<UINT32>(wcslen(kName)), name);
    }
    HRESULT STDMETHODCALLTYPE GetTrustLevel(TrustLevel* level) override {
        *level = BaseTrust;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetAt(UINT32 index, IInspectable** item) override {
        if (!item) return E_POINTER;
        const UINT32 n = real_size();
        if (index < n) return real_->GetAt(index, item);
        if (index == n) {
            pad_->AddRef();
            *item = pad_;
            return S_OK;
        }
        *item = nullptr;
        return E_BOUNDS;
    }
    HRESULT STDMETHODCALLTYPE get_Size(UINT32* size) override {
        if (!size) return E_POINTER;
        *size = real_size() + 1;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE IndexOf(IInspectable* item, UINT32* index, boolean* found) override {
        if (!index || !found) return E_POINTER;
        if (item == static_cast<IInspectable*>(pad_)) {
            *index = real_size();
            *found = true;
            return S_OK;
        }
        if (real_) return real_->IndexOf(item, index, found);
        *index = 0;
        *found = false;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetMany(UINT32 start, UINT32 capacity, IInspectable** items, UINT32* actual) override {
        if (!actual) return E_POINTER;
        *actual        = 0;
        const UINT32 n = real_size() + 1;
        for (UINT32 i = start; i < n && *actual < capacity; ++i) {
            if (FAILED(GetAt(i, &items[*actual]))) break;
            ++*actual;
        }
        return S_OK;
    }

private:
    ~View() {
        if (real_) real_->Release();
    }
    UINT32 real_size() const {
        UINT32 n = 0;
        if (real_ && FAILED(real_->get_Size(&n))) n = 0;
        return n;
    }
    std::atomic<ULONG> refs_{1};
    Ftm ftm_;
    IVectorViewRaw* real_; // owned (may be null: Windows.Gaming.Input wasn't there)
    KeyPad* pad_;
};

// The statics Azimuth gets: the real ones' events, and the list with the keyboard's pad in it.
class Statics final : public IGamepadStaticsRaw {
public:
    Statics(IGamepadStaticsRaw* real, KeyPad* pad) : real_(real), pad_(pad) { ftm_.make(static_cast<IGamepadStaticsRaw*>(this)); }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override {
        if (!out) return E_POINTER;
        if (iid == __uuidof(IMarshal)) return ftm_.query(iid, out);
        if (iid == __uuidof(IUnknown) || iid == __uuidof(IInspectable) || iid == kIAgileObject ||
            iid == kIGamepadStatics) {
            *out = static_cast<IGamepadStaticsRaw*>(this);
            AddRef();
            return S_OK;
        }
        if (real_) return real_->QueryInterface(iid, out); // IGamepadStatics2 and the like: the real ones
        *out = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs_; }
    ULONG STDMETHODCALLTYPE Release() override {
        const ULONG n = --refs_;
        if (n == 0) delete this;
        return n;
    }
    HRESULT STDMETHODCALLTYPE GetIids(ULONG* count, IID** out) override { return iids(count, out, kIGamepadStatics); }
    HRESULT STDMETHODCALLTYPE GetRuntimeClassName(HSTRING* name) override { return class_name(name); }
    HRESULT STDMETHODCALLTYPE GetTrustLevel(TrustLevel* level) override {
        *level = BaseTrust;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE add_GamepadAdded(IUnknown* handler, EventRegistrationToken* token) override {
        if (real_) return real_->add_GamepadAdded(handler, token);
        if (token) token->value = 0;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE remove_GamepadAdded(EventRegistrationToken token) override {
        return real_ ? real_->remove_GamepadAdded(token) : S_OK;
    }
    HRESULT STDMETHODCALLTYPE add_GamepadRemoved(IUnknown* handler, EventRegistrationToken* token) override {
        if (real_) return real_->add_GamepadRemoved(handler, token);
        if (token) token->value = 0;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE remove_GamepadRemoved(EventRegistrationToken token) override {
        return real_ ? real_->remove_GamepadRemoved(token) : S_OK;
    }
    HRESULT STDMETHODCALLTYPE get_Gamepads(IInspectable** out) override {
        if (!out) return E_POINTER;
        IVectorViewRaw* real_view = nullptr;
        if (real_) {
            IInspectable* v = nullptr;
            if (SUCCEEDED(real_->get_Gamepads(&v)) && v) real_view = static_cast<IVectorViewRaw*>(v);
        }
        *out = new View(real_view, pad_);
        lists_.fetch_add(1, std::memory_order_relaxed);
        return S_OK;
    }

    uint64_t lists() const { return lists_.load(); }

private:
    ~Statics() {
        if (real_) real_->Release();
    }
    std::atomic<ULONG> refs_{1};
    Ftm ftm_;
    IGamepadStaticsRaw* real_; // owned, may be null
    KeyPad* pad_;
    std::atomic<uint64_t> lists_{0};
};

} // namespace wgi
