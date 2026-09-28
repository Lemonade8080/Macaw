#pragma once

#include "Core/Base/UObject.h"
#include "Core/Base/FObjectHandle.h"
#include "Core/Base/UObjectSystem.h"
#include "./ErrorHandler.h"

#include <concepts>
#include <cstddef>

/// <summary>
/// GetItemCount: Object 담긴 리스트 전체 길이(빈 슬롯 포함)
///
/// </summary>
namespace UObjectSystem {
    enum class ETypeMatch {
        Exact,
        IncludeDerived
    };

    // UObject에서 파생된 타입만 사용가능
    template <typename TObject>
        requires std::derived_from<TObject, UObject>
    class TObjectIterator {
    public:
        // 순회 범위 (start, end - 1까지)
        TObjectIterator(Uint32 Start, Uint32 End);

        // 레퍼런스 리턴
        TObject& operator*() const;

        TObject* operator->() const;

        TObjectIterator& operator++();

        TObjectIterator& operator--();

        operator bool() const;

        // 후순위
        TObject& operator[](std::size_t Index);

        TObject& operator[](std::size_t Index) const;

        bool operator==(const TObjectIterator& Other) const;

        bool operator!=(const TObjectIterator& Other) const;

    private:
        bool AdvanceToNextValidObject();

        bool Advance();

        /*bool Advance()
        {
            ++_CurrentIndex;
            return UObjectSystem::TryGet(_CurrentIndex, _handle);
        }*/

        FObjectHandle mHandle{};

        Uint32 mCurrentIndex{};
        Uint32 mEndIndex{};
    };

    template <typename TObject> requires std::derived_from<TObject, UObject> class TObjectRange { public: TObjectRange(Uint32 End); TObjectIterator<TObject> begin() const; TObjectIterator<TObject> end() const; private: Uint32 mEnd{}; };

    template <typename TObject> requires std::derived_from<TObject, UObject> TObjectRange<TObject> Objects() {
        return TObjectRange<TObject>(UObjectSystem::GetItemCount());
    }

}

namespace UObjectSystem {
    template <typename TObject> requires std::derived_from<TObject, UObject> TObjectIterator<TObject>::TObjectIterator(Uint32 Start, Uint32 End)
        : mCurrentIndex(Start),
          mEndIndex(End) {
        if (Start > End)
            ErrorHandler::Report("Invalid start!", "Start > End", ErrorHandler::EErrorLevel::Critical);

        AdvanceToNextValidObject();
    }
}

namespace UObjectSystem {
    template <typename TObject> requires std::derived_from<TObject, UObject> TObject& TObjectIterator<TObject>::operator*() const {
        UObject* Object{UObjectSystem::Resolve(mHandle)};

        if (mCurrentIndex == mEndIndex || Object == nullptr ||
            !Object->GetTypeInfo()->IsA(TObject::StaticTypeInfo())) {
            ErrorHandler::Report("Refer Error!", "Does not refer to a live object", ErrorHandler::EErrorLevel::Critical);
        }

        return *static_cast<TObject*>(Object);
    }
}

namespace UObjectSystem {
    template <typename TObject> requires std::derived_from<TObject, UObject> TObject* TObjectIterator<TObject>::operator->() const {
        return &operator*();
    }
}

namespace UObjectSystem {
    template <typename TObject> requires std::derived_from<TObject, UObject> TObjectIterator<TObject>& TObjectIterator<TObject>::operator++() {
        if (mCurrentIndex < mEndIndex) {
            ++mCurrentIndex;
            AdvanceToNextValidObject();
        }
        return *this;
    }
}

namespace UObjectSystem {
    template <typename TObject> requires std::derived_from<TObject, UObject> TObjectIterator<TObject>& TObjectIterator<TObject>::operator--() {
        for (Uint32 Index{mCurrentIndex}; Index > 0;) {
            --Index;

            FObjectHandle Handle{};

            if (UObjectSystem::TryGet(Index, Handle)) {
                UObject* Object{UObjectSystem::Resolve(Handle)};

                // 같은 타입인지
                if (Object != nullptr && Object->GetTypeInfo()->IsA(TObject::StaticTypeInfo())) {
                    mCurrentIndex = Index;
                    mHandle = Handle;
                    return *this;
                }
            }
        }
        ErrorHandler::Report("Invalid decrement!", "No previous object", ErrorHandler::EErrorLevel::Critical);
        return *this;
    }
}

namespace UObjectSystem {
    template <typename TObject> requires std::derived_from<TObject, UObject> TObjectIterator<TObject>::operator bool() const {
        // return UObjectSystem::TryGet(_CurrentIndex, _handle);
        return mCurrentIndex < mEndIndex;
    }
}

namespace UObjectSystem {
    template <typename TObject> requires std::derived_from<TObject, UObject> TObject& TObjectIterator<TObject>::operator[](std::size_t Index) {
        TObjectIterator It{*this};

        for (std::size_t I{0}; I < Index; ++I) {
            ++It;
        }

        return *It;
    }
}

namespace UObjectSystem {
    template <typename TObject> requires std::derived_from<TObject, UObject> TObject& TObjectIterator<TObject>::operator[](std::size_t Index) const {
        TObjectIterator It{*this};

        for (std::size_t I{0}; I < Index; ++I)
            ++It;

        return *It;
    }
}

namespace UObjectSystem {
    template <typename TObject> requires std::derived_from<TObject, UObject> bool TObjectIterator<TObject>::operator==(const TObjectIterator& Other) const {
        return mCurrentIndex == Other.mCurrentIndex && mEndIndex == Other.mEndIndex;
    }
}

namespace UObjectSystem {
    template <typename TObject> requires std::derived_from<TObject, UObject> bool TObjectIterator<TObject>::operator!=(const TObjectIterator& Other) const {
        return !(*this == Other);
    }
}

namespace UObjectSystem {
    template <typename TObject> requires std::derived_from<TObject, UObject> bool TObjectIterator<TObject>::AdvanceToNextValidObject() {
        while (Advance()) {
            UObject* Object{UObjectSystem::Resolve(mHandle)};

            // 타입검사(완전히 같은 타입)
            if (Object != nullptr && Object->GetTypeInfo()->IsA(TObject::StaticTypeInfo()))
                return true;

            ++mCurrentIndex; // 핵심: 타입이 다르거나 빈 슬롯이면 다음 인덱스로
        }
        mCurrentIndex = mEndIndex;
        return false;
    }
}

namespace UObjectSystem {
    template <typename TObject> requires std::derived_from<TObject, UObject> bool TObjectIterator<TObject>::Advance() {
        while (mCurrentIndex < mEndIndex) {
            if (UObjectSystem::TryGet(mCurrentIndex, mHandle))
                return true; // 이 슬롯은 타입 검사를 해볼 수 있음

            ++mCurrentIndex; // 빈 슬롯이면 계속 탐색
        }

        mHandle = {}; // 초기화
        return false; // 범위의 끝
    }
}

namespace UObjectSystem {
    template <typename TObject> requires std::derived_from<TObject, UObject> TObjectRange<TObject>::TObjectRange(Uint32 End)
        : mEnd(End) {
    }
}

namespace UObjectSystem {
    template <typename TObject> requires std::derived_from<TObject, UObject> TObjectIterator<TObject> TObjectRange<TObject>::begin() const {
        return {0, mEnd};
    }
}

namespace UObjectSystem {
    template <typename TObject> requires std::derived_from<TObject, UObject> TObjectIterator<TObject> TObjectRange<TObject>::end() const {
        return {mEnd, mEnd};
    }
}
