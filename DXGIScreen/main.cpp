#include <string>
#include <iostream>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <list>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")


//����ID3D11Device��ID3D11DeviceContext����
bool InitD3D11Device(ID3D11Device** p_device, ID3D11DeviceContext** p_deviceContext)
{
    D3D_DRIVER_TYPE DriverTypes[] =
    {
        D3D_DRIVER_TYPE_HARDWARE,
        D3D_DRIVER_TYPE_WARP,
        D3D_DRIVER_TYPE_REFERENCE,
    };
    UINT NumDriverTypes = ARRAYSIZE(DriverTypes);

    D3D_FEATURE_LEVEL FeatureLevels[] =
    {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_1
    };
    UINT NumFeatureLevels = ARRAYSIZE(FeatureLevels);
    D3D_FEATURE_LEVEL FeatureLevel;

    for (UINT DriverTypeIndex = 0; DriverTypeIndex < NumDriverTypes; ++DriverTypeIndex)
    {
        HRESULT hr = D3D11CreateDevice(
            nullptr,
            DriverTypes[DriverTypeIndex],
            nullptr, 0,
            FeatureLevels,
            NumFeatureLevels,
            D3D11_SDK_VERSION,
            p_device,
            &FeatureLevel,
            p_deviceContext);
        if (SUCCEEDED(hr))
        {
            break;
        }
    }

    if (*p_device == nullptr || *p_deviceContext == nullptr)
    {
        return false;
    }

    return true;
}



//�õ�IDXGIOutputDuplication�б�
bool InitDuplication(ID3D11Device* device_, std::list<IDXGIOutputDuplication*>* p_duplications)
{
    HRESULT hr = S_OK;
    bool ret = true;
    IDXGIDevice* dxgiDevice = nullptr;
    hr = device_->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgiDevice));
    if (FAILED(hr))
    {
        return false;
    }

    IDXGIAdapter* dxgiAdapter = nullptr;
    hr = dxgiDevice->GetAdapter(&dxgiAdapter);
    dxgiDevice->Release();
    if (FAILED(hr))
    {
        return false;
    }

    UINT output = 0;
    IDXGIOutput* dxgiOutput = nullptr;

    (*p_duplications).push_back(nullptr);
    for (std::list<IDXGIOutputDuplication*>::iterator it = (*p_duplications).begin(); it != (*p_duplications).end(); ++it) {

        hr = dxgiAdapter->EnumOutputs(output++, &dxgiOutput);
        if (hr == DXGI_ERROR_NOT_FOUND)
        {
            (*p_duplications).pop_back();
            break;
        }
        else
        {
            DXGI_OUTPUT_DESC desc;
            dxgiOutput->GetDesc(&desc);
            int width = desc.DesktopCoordinates.right - desc.DesktopCoordinates.left;
            int height = desc.DesktopCoordinates.bottom - desc.DesktopCoordinates.top;
        }

        IDXGIOutput1* dxgiOutput1 = nullptr;
        hr = dxgiOutput->QueryInterface(__uuidof(IDXGIOutput1), reinterpret_cast<void**>(&dxgiOutput1));
        dxgiOutput->Release();
        if (FAILED(hr))
        {
            ret = false;
            break;
        }

        //��ȡIDXGIOutputDuplication����
        hr = dxgiOutput1->DuplicateOutput(device_, &(*it));
        dxgiOutput1->Release();
        if (FAILED(hr))
        {
            ret = false;
            break;
        }

        (*p_duplications).push_back(nullptr);
    }
    dxgiAdapter->Release();

    return ret;
}



int
GetDesktopFrame(
    ID3D11Device* device_,
    ID3D11DeviceContext* deviceContext_,
    std::list<IDXGIOutputDuplication*> duplications,
    std::list<ID3D11Texture2D*>* p_texture2Ds)
{
    HRESULT hr = S_OK;
    DXGI_OUTDUPL_FRAME_INFO frameInfo;
    IDXGIResource* resource = nullptr;
    ID3D11Texture2D* acquireFrame = nullptr;

    for (std::list<IDXGIOutputDuplication*>::iterator it = (duplications).begin();
        it != (duplications).end(); ++it) {

        hr = (*it)->AcquireNextFrame(0, &frameInfo, &resource);
        if (FAILED(hr))
        {
            if (hr == DXGI_ERROR_WAIT_TIMEOUT)
            {
                return -1;
            }
            else
            {
                return -1;
            }
        }

        hr = resource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&acquireFrame));
        resource->Release();
        if (FAILED(hr))
        {
            return -1;
        }

        D3D11_TEXTURE2D_DESC desc;
        acquireFrame->GetDesc(&desc);
        desc.Usage = D3D11_USAGE_STAGING;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        desc.BindFlags = 0;
        desc.MiscFlags = 0;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.SampleDesc.Count = 1;
        ID3D11Texture2D* texture2D = nullptr;
        device_->CreateTexture2D(&desc, NULL, &texture2D);
        (*p_texture2Ds).push_back(texture2D);
        if (texture2D)
        {
            deviceContext_->CopyResource(texture2D, acquireFrame);
        }
        acquireFrame->Release();

        hr = (*it)->ReleaseFrame();
        if (FAILED(hr))
        {
            return -1;
        }

    }
    return 0;
}

void SaveBmp(std::string filename, const uint8_t* data, int width, int height)
{
    HANDLE hFile = CreateFileA(
        filename.c_str(),
        GENERIC_WRITE, 0,
        NULL, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == NULL)
    {
        return;
    }
    // ��д���ֽ���
    DWORD bytesWritten = 0;
    // λͼ��С����ɫĬ��Ϊ32λ��rgba
    int bmpSize = width * height * 4;

    // �ļ�ͷ
    BITMAPFILEHEADER bmpHeader;
    // �ļ��ܴ�С = �ļ�ͷ + λͼ��Ϣͷ + λͼ����
    bmpHeader.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + bmpSize;
    // �̶�
    bmpHeader.bfType = 0x4D42;
    // ����ƫ�ƣ���λͼ��������λ��
    bmpHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    // ����Ϊ0
    bmpHeader.bfReserved1 = 0;
    // ����Ϊ0
    bmpHeader.bfReserved2 = 0;
    // д�ļ�ͷ
    WriteFile(hFile, (LPSTR)&bmpHeader, sizeof(bmpHeader), &bytesWritten, NULL);

    // λͼ��Ϣͷ
    BITMAPINFOHEADER bmiHeader;
    // λͼ��Ϣͷ��С
    bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    // λͼ���ؿ��
    bmiHeader.biWidth = width;
    // λͼ���ظ߶ȣ����߶ȼ����·�ת
    bmiHeader.biHeight = -height;
    // ����Ϊ1
    bmiHeader.biPlanes = 1;
    // ������ռλ��
    bmiHeader.biBitCount = 32;
    // 0��ʾ��ѹ��
    bmiHeader.biCompression = 0;
    // λͼ���ݴ�С
    bmiHeader.biSizeImage = bmpSize;
    // ˮƽ�ֱ���(����/��)
    bmiHeader.biXPelsPerMeter = 0;
    // ��ֱ�ֱ���(����/��)
    bmiHeader.biYPelsPerMeter = 0;
    // ʹ�õ���ɫ��0Ϊʹ��ȫ����ɫ
    bmiHeader.biClrUsed = 0;
    // ��Ҫ����ɫ����0Ϊ������ɫ����Ҫ
    bmiHeader.biClrImportant = 0;

    // дλͼ��Ϣͷ
    WriteFile(hFile, (LPSTR)&bmiHeader, sizeof(bmiHeader), &bytesWritten, NULL);
    // дλͼ����
    WriteFile(hFile, data, bmpSize, &bytesWritten, NULL);
    CloseHandle(hFile);
}




//���������������ݣ������ݴ�GPU�е����ݱ������ڴ���
int
StripDestopBitData(
    RECT* p_rect,
    uint8_t* bit_data,
    size_t* len,
    std::list<ID3D11Texture2D*> texture2Ds,
    ID3D11DeviceContext* deviceContext_)
{
    struct DESTOP_BAMPDATA {
        uint8_t* p_data;
        long width;
        long height;
    };
    D3D11_TEXTURE2D_DESC desc;
    std::list<DESTOP_BAMPDATA> destop_bampdatas;

    //�������е�ID3D11Texture2D���󲢻�ȡ����������
    for (std::list<ID3D11Texture2D*>::iterator texture2D_it = (texture2Ds).begin();
        texture2D_it != (texture2Ds).end(); ++texture2D_it) {
        (*texture2D_it)->GetDesc(&desc);
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        deviceContext_->Map((*texture2D_it), 0, D3D11_MAP_READ, 0, &mappedResource);
        size_t imageSize = desc.Width * desc.Height * 4;
        uint8_t* rgba = (uint8_t*)malloc(imageSize);
        if (rgba != nullptr) {
            memset(rgba, 0, imageSize);
            uint8_t* pData = (uint8_t*)mappedResource.pData;
            for (size_t i = 0; i < desc.Height; i++) {
                memcpy(rgba + i * desc.Width * 4, pData + i * mappedResource.RowPitch, desc.Width * 4);
            }
            DESTOP_BAMPDATA destop_data = { 0 };
            destop_data.p_data = rgba;
            destop_data.width = desc.Width;
            destop_data.height = desc.Height;
            destop_bampdatas.push_back(destop_data);
        }
        deviceContext_->Unmap((*texture2D_it), 0);
    }

    //�õ���������߶��Լ��ܿ��
    long max_height = 0;
    long sum_width = 0;
    for (std::list<DESTOP_BAMPDATA>::iterator it = destop_bampdatas.begin();
        it != destop_bampdatas.end(); ++it) {
        if ((*it).height > max_height) {
            max_height = (*it).height;
        }
        sum_width += (*it).width;
    }


    bool ret = 0;
    //�Դ�����Ļ��Χ�ڵ�������вü���
    if ((*p_rect).left < sum_width &&
        (*p_rect).top < max_height &&
        (*p_rect).right > 0 &&
        (*p_rect).bottom > 0) {

        uint8_t* sum_destop_data = (uint8_t*)malloc(sum_width * max_height * 4);
        if (sum_destop_data) {
            //������������������κϲ�
            memset(sum_destop_data, 0, sum_width * max_height * 4);
            for (int i = 0; i < max_height; i++) {
                UINT them_width = 0;
                for (std::list<DESTOP_BAMPDATA>::iterator it = destop_bampdatas.begin();
                    it != destop_bampdatas.end(); ++it) {
                    if (i > (*it).height - 1) {
                        //����������0���
                        memset(sum_destop_data + (i * sum_width + them_width) * 4, 0, (*it).width * 4);
                    }
                    else {
                        memcpy(sum_destop_data + (i * sum_width + them_width) * 4,
                            (*it).p_data + i * (*it).width * 4, (*it).width * 4);
                    }
                    them_width += (*it).width;
                }
            }

            //��ȡָ��������������,ͨ������Ļ���ݽ��вü�
            if ((*p_rect).right > sum_width)
                (*p_rect).right = sum_width;
            if ((*p_rect).bottom > max_height)
                (*p_rect).bottom = max_height;
            if ((*p_rect).left < 0)
                (*p_rect).left = 0;
            if ((*p_rect).top < 0)
                (*p_rect).top = 0;
            size_t Clild_width = (*p_rect).right - (*p_rect).left;
            size_t Clild_hight = (*p_rect).bottom - (*p_rect).top;
            size_t Child_image_size = Clild_width * Clild_hight * 4;
            for (size_t i = (*p_rect).top; i < (*p_rect).bottom; i++)
            {
                memcpy(bit_data + (i - (*p_rect).top) * Clild_width * 4,
                    sum_destop_data + (i * sum_width + (*p_rect).left) * 4,
                    Clild_width * 4);
            }
            free(sum_destop_data);
            *len = Child_image_size;
        }
        //�жϻ�ȡ�������Ƿ���Ч
        size_t old_len = *len;
        *len = 0;
        for (int i = 0; i < old_len / 4; i++) {
            if (*(uint32_t*)(bit_data + i * 4) != 0) {
                *len = old_len;
                break;
            }
        }
        ret = 0;
    }
    else {
        //rcet������Ļ��Χ������Ч
        ret = 1;
    }

    //�ͷ������������������
    for (std::list<DESTOP_BAMPDATA>::iterator it = destop_bampdatas.begin();
        it != destop_bampdatas.end(); ++it) {
        free((*it).p_data);
    }
    destop_bampdatas.clear();
    return ret;
}

//�ͷ���Դ
int
DeleteResource(
    ID3D11Device* device_,
    ID3D11DeviceContext* deviceContext_,
    std::list<IDXGIOutputDuplication*>* p_duplications)
{
    device_->Release();
    deviceContext_->Release();
    for (std::list<IDXGIOutputDuplication*>::iterator it = (*p_duplications).begin();
        it != (*p_duplications).end(); ++it) {
        if (*it) {
            (*it)->Release();
        }
    }
    (*p_duplications).clear();
    return 0;
}


//bit_dataΪ��ȡ����������
//lenΪʵ�ʻ�ȡ���������ݴ�С
//flag = 1���г�ʼ���� flag = 0������ȡ��������
bool GetRectBitData(RECT* p_rect, uint8_t* bit_data, size_t* len, int flag)
{
    if (NULL == p_rect || NULL == bit_data || NULL == len) return false;

    int ret = true;
    static ID3D11Device* device_ = nullptr;
    static ID3D11DeviceContext* deviceContext_ = nullptr;
    static std::list<IDXGIOutputDuplication*> duplications;    //IDXGIOutputDuplication�б�
    static std::list<ID3D11Texture2D*> texture2Ds;             //ID3D11Texture2D�б�
    if (flag == 1) {
        //���³�ʼ��
        InitD3D11Device(&device_, &deviceContext_);
        InitDuplication(device_, &duplications);
    }
    ret = GetDesktopFrame(device_, deviceContext_, duplications, &texture2Ds);
    if (0 == ret) {
        //�������ȡָ���������������
        ret = StripDestopBitData(p_rect, bit_data, len, texture2Ds, deviceContext_);
    }

    //�������е�texture2Ds֡����
    for (std::list<ID3D11Texture2D*>::iterator texture2D_it = (texture2Ds).begin();
        texture2D_it != (texture2Ds).end(); ++texture2D_it) {
        if (*texture2D_it) {
            (*texture2D_it)->Release();
        }
    }
    texture2Ds.clear();
    //�����ȡʧ�ܾ��������е����ݲ����³�ʼ����ȡ
    if (-1 == ret) {
        DeleteResource(device_, deviceContext_, &duplications);
        return GetRectBitData(p_rect, bit_data, len, 1);
    }
    else if (ret == 0 && *len == 0) {
        //��ȡ�ɹ���������Ч�����»�ȡ���������³�ʼ��
        return GetRectBitData(p_rect, bit_data, len, 0);
    }
    else if (ret == 1) {
        //rect��Ч�򷵻�ʧ��
        DeleteResource(device_, deviceContext_, &duplications);
        return false;
    }
    return true;
}


int main()
{

    //����ʾ��
    int flag = 1;
    while (1) {
        RECT rect = { 0 };
        HWND hwnd = GetForegroundWindow();
        GetWindowRect(hwnd, &rect);

        size_t len = 0;
        uint8_t* p_data = (uint8_t*)malloc((rect.right - rect.left) * (rect.bottom - rect.top) * 4);
        if (p_data) {
            memset(p_data, 0, (rect.right - rect.left) * (rect.bottom - rect.top) * 4);
            if (true == GetRectBitData(&rect, p_data, &len, flag)) {
                //��ȡ�ɹ���ֱ��ֱ��flag = 0������ȡ����֡����
                flag = 0;
            }
            else {
                //��ȡʧ������Ҫ���³�ʼ��flag = 1��
                flag = 1;
            }
            //����Ϊbmp�ļ�
            SaveBmp("1.bmp", p_data, (rect.right - rect.left), (rect.bottom - rect.top));
            free(p_data);
        }
        Sleep(1);
    }
}
