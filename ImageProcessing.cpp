#include "pch.h"
#include "ImageProcessing.h"

namespace image_channel_viewer::image_processing
{
    winrt::Windows::Graphics::Imaging::SoftwareBitmap CreateSoftwareBitmapFromPixels(
        ::image_channel_viewer::imaging::ContinuousPixelBuffer& pixels,
        uint32_t pixelWidth,
        uint32_t pixelHeight)
    {
        winrt::Windows::Graphics::Imaging::SoftwareBitmap softwareBitmap(
            winrt::Windows::Graphics::Imaging::BitmapPixelFormat::Bgra8,
            static_cast<int32_t>(pixelWidth),
            static_cast<int32_t>(pixelHeight),
            winrt::Windows::Graphics::Imaging::BitmapAlphaMode::Premultiplied);

        auto buffer = softwareBitmap.LockBuffer(winrt::Windows::Graphics::Imaging::BitmapBufferAccessMode::Write);
        auto reference = buffer.CreateReference();
        auto byteAccess = reference.as<IMemoryBufferByteAccess>();

        uint8_t* destination = nullptr;
        uint32_t capacity = 0;
        winrt::check_hresult(byteAccess->GetBuffer(&destination, &capacity));

        const auto plane = buffer.GetPlaneDescription(0);
        std::copy(pixels.winrt_begin(), pixels.winrt_end(), destination + plane.StartIndex);
        return softwareBitmap;
    }
}