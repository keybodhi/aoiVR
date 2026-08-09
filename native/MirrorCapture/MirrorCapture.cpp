#include <windows.h>
#include <d3d11.h>
#include <stdio.h>
#include <stdlib.h>

#pragma comment(lib, "d3d11.lib")

extern "C" {

// Reads back the OpenVR mirror texture given device/ctx/srv and writes a 32-bit BGRA BMP file.
// Returns 0 on success, negative error code on failure.
__declspec(dllexport) int MirrorShot(void* pDevice, void* pCtx, void* pSrv, const wchar_t* outBmpPath)
{
    if (!pDevice || !pCtx || !pSrv || !outBmpPath) return -1;
    ID3D11Device* dev = (ID3D11Device*)pDevice;
    ID3D11DeviceContext* ctx = (ID3D11DeviceContext*)pCtx;
    ID3D11ShaderResourceView* srv = (ID3D11ShaderResourceView*)pSrv;

    ID3D11Resource* res = NULL;
    srv->GetResource(&res);
    if (!res) return -2;

    ID3D11Texture2D* tex2D = NULL;
    HRESULT hr = res->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&tex2D);
    if (FAILED(hr) || !tex2D) { res->Release(); return -3; }

    D3D11_TEXTURE2D_DESC desc;
    tex2D->GetDesc(&desc);
    if (desc.Width == 0 || desc.Height == 0) { tex2D->Release(); res->Release(); return -4; }

    D3D11_TEXTURE2D_DESC sd = desc;
    sd.Usage = D3D11_USAGE_STAGING;
    sd.BindFlags = 0;
    sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    sd.MiscFlags = 0;

    ID3D11Texture2D* staging = NULL;
    hr = dev->CreateTexture2D(&sd, NULL, &staging);
    if (FAILED(hr) || !staging) { tex2D->Release(); res->Release(); return -5; }

    ctx->CopyResource(staging, res);

    D3D11_MAPPED_SUBRESOURCE mapped;
    hr = ctx->Map(staging, 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) { staging->Release(); tex2D->Release(); res->Release(); return -6; }

    unsigned int w = desc.Width;
    unsigned int h = desc.Height;

    // BMP: 14-byte header + 40-byte DIB + BGRA pixels (bottom-up), 4 bytes per pixel.
    unsigned int rowBytes = w * 4;
    unsigned int fileSize = 54 + rowBytes * h;
    unsigned char* bmp = (unsigned char*)malloc(fileSize);
    if (!bmp) { ctx->Unmap(staging, 0); staging->Release(); tex2D->Release(); res->Release(); return -7; }
    memset(bmp, 0, fileSize);

    bmp[0] = 'B'; bmp[1] = 'M';
    bmp[2] = (unsigned char)(fileSize & 0xFF); bmp[3] = (unsigned char)((fileSize >> 8) & 0xFF);
    bmp[4] = (unsigned char)((fileSize >> 16) & 0xFF); bmp[5] = (unsigned char)((fileSize >> 24) & 0xFF);
    bmp[10] = 54;
    bmp[14] = 40;
    bmp[18] = (unsigned char)(w & 0xFF); bmp[19] = (unsigned char)((w >> 8) & 0xFF);
    bmp[20] = (unsigned char)((w >> 16) & 0xFF); bmp[21] = (unsigned char)((w >> 24) & 0xFF);
    bmp[22] = (unsigned char)(h & 0xFF); bmp[23] = (unsigned char)((h >> 8) & 0xFF);
    bmp[24] = (unsigned char)((h >> 16) & 0xFF); bmp[25] = (unsigned char)((h >> 24) & 0xFF);
    bmp[26] = 1;
    bmp[28] = 32;
    bmp[34] = rowBytes * h;

    // Copy bottom-up, converting RGBA to BGRA if needed.
    const unsigned char* src = (const unsigned char*)mapped.pData;
    unsigned int srcPitch = mapped.RowPitch;
    for (unsigned int y = 0; y < h; y++) {
        const unsigned char* srow = src + (unsigned long long)y * srcPitch;
        unsigned char* drow = bmp + 54 + (h - 1 - y) * rowBytes;
        if (desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM) {
            for (unsigned int x = 0; x < w; x++) {
                drow[x * 4 + 0] = srow[x * 4 + 2];
                drow[x * 4 + 1] = srow[x * 4 + 1];
                drow[x * 4 + 2] = srow[x * 4 + 0];
                drow[x * 4 + 3] = 255;
            }
        } else {
            memcpy(drow, srow, rowBytes);
        }
    }

    ctx->Unmap(staging, 0);

    FILE* f = NULL;
    _wfopen_s(&f, outBmpPath, L"wb");
    int rc = 0;
    if (f) {
        fwrite(bmp, 1, fileSize, f);
        fclose(f);
    } else {
        rc = -8;
    }
    free(bmp);

    staging->Release();
    tex2D->Release();
    res->Release();
    return rc;
}

}
