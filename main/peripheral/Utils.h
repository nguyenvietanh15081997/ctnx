#pragma once

#include <type_traits> // For std::underlying_type_t
#include <vector>

#define BUTTON_INDEX_1 0
#define BUTTON_INDEX_2 1
#define BUTTON_INDEX_3 2
#define BUTTON_INDEX_4 3

#define ELEMENT_ID_1 0x0001
#define ELEMENT_ID_2 0x0002
#define ELEMENT_ID_3 0x0003
#define ELEMENT_ID_4 0x0004
#define ELEMENT_ID_UNKNOWN 0x0000

namespace Utils {
    void convert_hsv_to_rgb(double H, double S, double V, uint8_t *Red, uint8_t *Green, uint8_t *Blue, uint8_t *Dim);
    
    constexpr uint16_t indexToElemId(uint8_t btnIndex) noexcept {
        switch (btnIndex)
        {
        case BUTTON_INDEX_1:
            return ELEMENT_ID_1;
        case BUTTON_INDEX_2:
            return ELEMENT_ID_UNKNOWN;
        case BUTTON_INDEX_3:
            return ELEMENT_ID_2;
        case BUTTON_INDEX_4:
            return ELEMENT_ID_3;
        default:
            return ELEMENT_ID_UNKNOWN;
        }
    }

    constexpr uint16_t elemIdToIndex(uint16_t elemId) noexcept {
        switch (elemId)
        {
        case ELEMENT_ID_1:
            return BUTTON_INDEX_1;
        case ELEMENT_ID_2:
            return BUTTON_INDEX_3;
        case ELEMENT_ID_3:
            return BUTTON_INDEX_4;
        case ELEMENT_ID_UNKNOWN:
            return BUTTON_INDEX_2;
        default:
            return BUTTON_INDEX_2;
        }
    }

    /**
     * @brief chuyển đổi enum sang kiểu nguyên thủy underlying type (thường là int)
     * ko ném exeption
     * auto: trả về tự động, ở đây trả về kiểu std::underlying_type_t<E>
     */
    template<typename EnumType>
    constexpr auto toIndex(EnumType e) noexcept {
        return static_cast<std::underlying_type_t<EnumType>>(e);
    }

    template<typename T>
    void append(std::vector<uint8_t>& buf, const T& val){
        const uint8_t *p = reinterpret_cast<const uint8_t *>(&val); //reinterpret_cast chuyên dùng để ép kiểu con trỏ
        buf.insert(buf.end(), p, p + sizeof(T)); // vec.insert(pos, first_iterator, last_iterator)
    }
    
    template<typename U>
    U read(uint8_t* &ptr){
        U val;
        memcpy(&val, ptr, sizeof(U)); ptr += sizeof(U);
        return val;
    }

}