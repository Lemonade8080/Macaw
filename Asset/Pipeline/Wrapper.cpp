#include "pch.h"
#include "Wrapper.h"

const rapidjson::Value* GetObject(const rapidjson::Value& Parent, const char* Name) {
    if (!Parent.IsObject() || !Parent.HasMember(Name) || !Parent[Name].IsObject()) {
        ErrorHandler::Report("GetObject", "A required JSON object is missing or invalid.", ErrorHandler::EErrorLevel::Error);
        return nullptr;
    }

    return &Parent[Name];
}

const rapidjson::Value* GetArray(const rapidjson::Value& Parent, const char* Name) {
    if (!Parent.IsObject() || !Parent.HasMember(Name) || !Parent[Name].IsArray()) {
        ErrorHandler::Report("GetArray", "A required JSON array is missing or invalid.", ErrorHandler::EErrorLevel::Error);
        return nullptr;
    }

    return &Parent[Name];
}

const char* GetString(const rapidjson::Value& Parent, const char* Name) {
    if (!Parent.IsObject() || !Parent.HasMember(Name) || !Parent[Name].IsString()) {
        ErrorHandler::Report("GetString", "A required JSON string is missing or invalid.", ErrorHandler::EErrorLevel::Error);
        return nullptr;
    }

    return Parent[Name].GetString();
}

const char* GetString(const rapidjson::Value& Parent, const char* Name, const char* DefaultValue) {
    if (!Parent.IsObject()) {
        ErrorHandler::Report("GetString", "The parent JSON value is not an object.", ErrorHandler::EErrorLevel::Error);
        return DefaultValue;
    }

    if (!Parent.HasMember(Name)) {
        return DefaultValue;
    }

    if (!Parent[Name].IsString()) {
        ErrorHandler::Report("GetString", "The JSON value is not a string.", ErrorHandler::EErrorLevel::Error);
        return DefaultValue;
    }

    return Parent[Name].GetString();
}

bool GetBool(const rapidjson::Value& Parent, const char* Name, bool DefaultValue) {
    if (!Parent.IsObject()) {
        ErrorHandler::Report("GetBool", "The parent JSON value is not an object.", ErrorHandler::EErrorLevel::Error);
        return DefaultValue;
    }

    if (!Parent.HasMember(Name)) {
        return DefaultValue;
    }

    if (!Parent[Name].IsBool()) {
        ErrorHandler::Report("GetBool", "The JSON value is not a Boolean.", ErrorHandler::EErrorLevel::Error);
        return DefaultValue;
    }

    return Parent[Name].GetBool();
}

unsigned int GetUint(const rapidjson::Value& Parent, const char* Name, unsigned int DefaultValue) {
    if (!Parent.IsObject()) {
        ErrorHandler::Report("GetUint", "The parent JSON value is not an object.", ErrorHandler::EErrorLevel::Error);
        return DefaultValue;
    }

    if (!Parent.HasMember(Name)) {
        return DefaultValue;
    }

    if (!Parent[Name].IsUint()) {
        ErrorHandler::Report("GetUint", "The JSON value is not an unsigned integer.", ErrorHandler::EErrorLevel::Error);
        return DefaultValue;
    }

    return Parent[Name].GetUint();
}

UINT GetUint(const rapidjson::Value& Parent, const char* Name) {
    if (!Parent.IsObject() || !Parent.HasMember(Name) || !Parent[Name].IsUint()) {
        ErrorHandler::Report("GetUint", "A required JSON unsigned integer is missing or invalid.", ErrorHandler::EErrorLevel::Error);
        return 0;
    }

    return Parent[Name].GetUint();
}

EVertexFormat ParseVertexFormat(const char* Value) {
    if (std::strcmp(Value, "Float2") == 0) {
        return EVertexFormat::Float2;
    }
    if (std::strcmp(Value, "Float3") == 0) {
        return EVertexFormat::Float3;
    }
    if (std::strcmp(Value, "Float4") == 0) {
        return EVertexFormat::Float4;
    }
    if (std::strcmp(Value, "Uint") == 0) {
        return EVertexFormat::Uint;
    }

    ErrorHandler::Report("ParseVertexFormat", "The vertex format is invalid.", ErrorHandler::EErrorLevel::Error);
    return EVertexFormat::Float3;
}

DXGI_FORMAT ConvertVertexFormat(EVertexFormat Value) {
    switch (Value) {
        case EVertexFormat::Float2:
            return DXGI_FORMAT_R32G32_FLOAT;
        case EVertexFormat::Float3:
            return DXGI_FORMAT_R32G32B32_FLOAT;
        case EVertexFormat::Float4:
            return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case EVertexFormat::Uint:
            return DXGI_FORMAT_R32_UINT;
        default:
            ErrorHandler::Report("ConvertVertexFormat", "The vertex format is invalid.", ErrorHandler::EErrorLevel::Error);
            return DXGI_FORMAT_UNKNOWN;
    }
}

EPrimitiveTopology ParsePrimitiveTopology(const char* Value) {
    if (std::strcmp(Value, "PointList") == 0)
        return EPrimitiveTopology::PointList;
    if (std::strcmp(Value, "LineList") == 0)
        return EPrimitiveTopology::LineList;
    if (std::strcmp(Value, "LineStrip") == 0)
        return EPrimitiveTopology::LineStrip;
    if (std::strcmp(Value, "TriangleList") == 0)
        return EPrimitiveTopology::TriangleList;
    if (std::strcmp(Value, "TriangleStrip") == 0)
        return EPrimitiveTopology::TriangleStrip;

    ErrorHandler::Report("ParsePrimitiveTopology", "The primitive topology is invalid.", ErrorHandler::EErrorLevel::Error);
    return EPrimitiveTopology::TriangleList;
}

D3D11_PRIMITIVE_TOPOLOGY ConvertPrimitiveTopology(EPrimitiveTopology Value) {
    switch (Value) {
        case EPrimitiveTopology::PointList:
            return D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
        case EPrimitiveTopology::LineList:
            return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
        case EPrimitiveTopology::LineStrip:
            return D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
        case EPrimitiveTopology::TriangleList:
            return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        case EPrimitiveTopology::TriangleStrip:
            return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
        default:
            ErrorHandler::Report("ConvertPrimitiveTopology", "The primitive topology is invalid.", ErrorHandler::EErrorLevel::Error);
            return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    }
}

EFillMode ParseFillMode(const char* Value) {
    if (std::strcmp(Value, "Solid") == 0)
        return EFillMode::Solid;
    if (std::strcmp(Value, "Wireframe") == 0)
        return EFillMode::Wireframe;

    ErrorHandler::Report("ParseFillMode", "The fill mode is invalid.", ErrorHandler::EErrorLevel::Error);
    return EFillMode::Solid;
}

D3D11_FILL_MODE ConvertFillMode(EFillMode Value) {
    switch (Value) {
        case EFillMode::Solid:
            return D3D11_FILL_SOLID;
        case EFillMode::Wireframe:
            return D3D11_FILL_WIREFRAME;
        default:
            ErrorHandler::Report("ConvertFillMode", "The fill mode is invalid.", ErrorHandler::EErrorLevel::Error);
            return D3D11_FILL_SOLID;
    }
}

ECullMode ParseCullMode(const char* Value) {
    if (std::strcmp(Value, "None") == 0)
        return ECullMode::None;
    if (std::strcmp(Value, "Front") == 0)
        return ECullMode::Front;
    if (std::strcmp(Value, "Back") == 0)
        return ECullMode::Back;

    ErrorHandler::Report("ParseCullMode", "The cull mode is invalid.", ErrorHandler::EErrorLevel::Error);
    return ECullMode::Back;
}

D3D11_CULL_MODE ConvertCullMode(ECullMode Value) {
    switch (Value) {
        case ECullMode::None:
            return D3D11_CULL_NONE;
        case ECullMode::Front:
            return D3D11_CULL_FRONT;
        case ECullMode::Back:
            return D3D11_CULL_BACK;
        default:
            ErrorHandler::Report("ConvertCullMode", "The cull mode is invalid.", ErrorHandler::EErrorLevel::Error);
            return D3D11_CULL_BACK;
    }
}

ECompareFunc ParseCompareFunc(const char* Value) {
    if (std::strcmp(Value, "Never") == 0)
        return ECompareFunc::Never;
    if (std::strcmp(Value, "Less") == 0)
        return ECompareFunc::Less;
    if (std::strcmp(Value, "Equal") == 0)
        return ECompareFunc::Equal;
    if (std::strcmp(Value, "LessEqual") == 0)
        return ECompareFunc::LessEqual;
    if (std::strcmp(Value, "Greater") == 0)
        return ECompareFunc::Greater;
    if (std::strcmp(Value, "NotEqual") == 0)
        return ECompareFunc::NotEqual;
    if (std::strcmp(Value, "GreaterEqual") == 0)
        return ECompareFunc::GreaterEqual;
    if (std::strcmp(Value, "Always") == 0)
        return ECompareFunc::Always;

    ErrorHandler::Report("ParseCompareFunc", "The comparison function is invalid.", ErrorHandler::EErrorLevel::Error);
    return ECompareFunc::Always;
}

D3D11_COMPARISON_FUNC ConvertCompareFunc(ECompareFunc Value) {
    switch (Value) {
        case ECompareFunc::Never:
            return D3D11_COMPARISON_NEVER;
        case ECompareFunc::Less:
            return D3D11_COMPARISON_LESS;
        case ECompareFunc::Equal:
            return D3D11_COMPARISON_EQUAL;
        case ECompareFunc::LessEqual:
            return D3D11_COMPARISON_LESS_EQUAL;
        case ECompareFunc::Greater:
            return D3D11_COMPARISON_GREATER;
        case ECompareFunc::NotEqual:
            return D3D11_COMPARISON_NOT_EQUAL;
        case ECompareFunc::GreaterEqual:
            return D3D11_COMPARISON_GREATER_EQUAL;
        case ECompareFunc::Always:
            return D3D11_COMPARISON_ALWAYS;
        default:
            ErrorHandler::Report("ConvertCompareFunc", "The comparison function is invalid.", ErrorHandler::EErrorLevel::Error);
            return D3D11_COMPARISON_ALWAYS;
    }
}

D3D11_STENCIL_OP ConvertStencillOp(EStencillOp Value) {
    switch (Value) {
        case EStencillOp::Keep:
            return D3D11_STENCIL_OP_KEEP;
        case EStencillOp::Zero:
            return D3D11_STENCIL_OP_ZERO;
        case EStencillOp::Replace:
            return D3D11_STENCIL_OP_REPLACE;
        case EStencillOp::IncrementClamp:
            return D3D11_STENCIL_OP_INCR_SAT;
        case EStencillOp::IncrementWrap:
            return D3D11_STENCIL_OP_INCR;
        case EStencillOp::DecrementClamp:
            return D3D11_STENCIL_OP_DECR_SAT;
        case EStencillOp::DecrementWrap:
            return D3D11_STENCIL_OP_DECR;
        case EStencillOp::Invert:
            return D3D11_STENCIL_OP_INVERT;
        default:
            ErrorHandler::Report("ConvertCompareFunc", "The comparison function is invalid.", ErrorHandler::EErrorLevel::Error);
            return D3D11_STENCIL_OP_KEEP;
    }
}

EBlend ParseBlend(const char* Value) {
    if (std::strcmp(Value, "Zero") == 0)
        return EBlend::Zero;
    if (std::strcmp(Value, "One") == 0)
        return EBlend::One;
    if (std::strcmp(Value, "SrcAlpha") == 0)
        return EBlend::SrcAlpha;
    if (std::strcmp(Value, "InvSrcAlpha") == 0)
        return EBlend::InvSrcAlpha;
    if (std::strcmp(Value, "DestAlpha") == 0)
        return EBlend::DestAlpha;
    if (std::strcmp(Value, "InvDestAlpha") == 0)
        return EBlend::InvDestAlpha;
    if (std::strcmp(Value, "SrcColor") == 0)
        return EBlend::SrcColor;
    if (std::strcmp(Value, "InvSrcColor") == 0)
        return EBlend::InvSrcColor;
    if (std::strcmp(Value, "DestColor") == 0)
        return EBlend::DestColor;
    if (std::strcmp(Value, "InvDestColor") == 0)
        return EBlend::InvDestColor;

    ErrorHandler::Report("ParseBlend", "The blend value is invalid.", ErrorHandler::EErrorLevel::Error);
    return EBlend::One;
}

D3D11_BLEND ConvertBlend(EBlend Value) {
    switch (Value) {
        case EBlend::Zero:
            return D3D11_BLEND_ZERO;
        case EBlend::One:
            return D3D11_BLEND_ONE;
        case EBlend::SrcAlpha:
            return D3D11_BLEND_SRC_ALPHA;
        case EBlend::InvSrcAlpha:
            return D3D11_BLEND_INV_SRC_ALPHA;
        case EBlend::DestAlpha:
            return D3D11_BLEND_DEST_ALPHA;
        case EBlend::InvDestAlpha:
            return D3D11_BLEND_INV_DEST_ALPHA;
        case EBlend::SrcColor:
            return D3D11_BLEND_SRC_COLOR;
        case EBlend::InvSrcColor:
            return D3D11_BLEND_INV_SRC_COLOR;
        case EBlend::DestColor:
            return D3D11_BLEND_DEST_COLOR;
        case EBlend::InvDestColor:
            return D3D11_BLEND_INV_DEST_COLOR;
        default:
            ErrorHandler::Report("ConvertBlend", "The blend value is invalid.", ErrorHandler::EErrorLevel::Error);
            return D3D11_BLEND_ONE;
    }
}

EBlendOp ParseBlendOp(const char* Value) {
    if (std::strcmp(Value, "Add") == 0)
        return EBlendOp::Add;
    if (std::strcmp(Value, "Subtract") == 0)
        return EBlendOp::Subtract;
    if (std::strcmp(Value, "RevSubtract") == 0)
        return EBlendOp::RevSubtract;
    if (std::strcmp(Value, "Min") == 0)
        return EBlendOp::Min;
    if (std::strcmp(Value, "Max") == 0)
        return EBlendOp::Max;

    ErrorHandler::Report("ParseBlendOp", "The blend operation is invalid.", ErrorHandler::EErrorLevel::Error);
    return EBlendOp::Add;
}

D3D11_BLEND_OP ConvertBlendOp(EBlendOp Value) {
    switch (Value) {
        case EBlendOp::Add:
            return D3D11_BLEND_OP_ADD;
        case EBlendOp::Subtract:
            return D3D11_BLEND_OP_SUBTRACT;
        case EBlendOp::RevSubtract:
            return D3D11_BLEND_OP_REV_SUBTRACT;
        case EBlendOp::Min:
            return D3D11_BLEND_OP_MIN;
        case EBlendOp::Max:
            return D3D11_BLEND_OP_MAX;
        default:
            ErrorHandler::Report("ConvertBlendOp", "The blend operation is invalid.", ErrorHandler::EErrorLevel::Error);
            return D3D11_BLEND_OP_ADD;
    }
}
