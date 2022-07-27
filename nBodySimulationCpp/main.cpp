#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include "d3dx12.h"
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <DirectXTK/WICTextureLoader.h>
#include <DirectXTK/SpriteBatch.h>
#include <DirectXTK/SpriteFont.h>
#include <map>
#include <time.h>
#include <dxgidebug.h>
#include <iostream>
#include <comdef.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "D3DCompiler.lib")

#ifndef HID_USAGE_PAGE_GENERIC
#define HID_USAGE_PAGE_GENERIC         ((USHORT) 0x01)
#endif
#ifndef HID_USAGE_GENERIC_MOUSE
#define HID_USAGE_GENERIC_MOUSE        ((USHORT) 0x02)
#endif

/*TODO
+1. Add resources releasing
2. Add errors handling
+3. Minimize the global state
+4. Add automatic full screen
+5. Add FPS counter
6. Add automatic GPU threads amount calculating
7. Add GUI
8. Replace Triangles with circles
9. Add gloving
10. Add interactive part
11. Split code into files
12. Render real objects
+13. Fix Release configuration
*/

//TODO: Move it into some out of global state
bool stopSimulation = true;
BOOL requestToRestartGame = false;
BOOL keysPressed[1024];
INT mouseX;
INT mouseY;
FLOAT yaw = 90.0f;
FLOAT pitch = 0;
FLOAT fakeObjectsSpeed = 30.0f;
DirectX::XMVECTOR cameraPos = { 0.0f, 0.0f, -60.0f, 0.0f };
DirectX::XMVECTOR cameraFront = { 0.0f, 0.0f, 1.0f, 0.0f };
DirectX::XMVECTOR cameraUp = { 0.0f, 1.0f, 0.0f, 0.0f };
//
//struct SpaceObject
//{
//	DirectX::XMFLOAT3 position;
//	DirectX::XMFLOAT3 velocity;
//};

struct CB_VS_vertexshader
{
	DirectX::XMMATRIX projectionMatrix;
	DirectX::XMMATRIX viewMatrix;
};

struct RealObjectVertexPrimitive
{
	DirectX::XMFLOAT3 position;
	DirectX::XMFLOAT2 texturePosition;
};

struct VertexShader
{
	ID3D11VertexShader* shader;
	ID3D11InputLayout* inputLayout;
};

struct PixelShader
{
	ID3D11PixelShader* shader;
};

struct GeometryShader
{
	ID3D11GeometryShader* shader;
};

struct ComputeShader
{
	ID3D11ComputeShader* shader;
};

struct ShadersList
{
	std::map<std::string, VertexShader> vertexShaders;
	std::map<std::string, PixelShader> pixelShaders;
	std::map<std::string, GeometryShader> geometryShaders;
	std::map<std::string, ComputeShader> computeShaders;
};

struct Vertex
{
	Vertex(float x, float y, float z, float u, float w) : position(x, y, z) {}
	//Vertex(float x, float y, float z, float u, float w) : position(x, y, z), textureCoord(u, w) {}
	DirectX::XMFLOAT3 position;
	//DirectX::XMFLOAT2 textureCoord;
};

struct SingleBuffer
{
	ID3D11Buffer* buffer;
	UINT length;
	UINT strideLength;
};

struct BuffersList
{
	SingleBuffer fakeObjectsPositionsBuffer;
	SingleBuffer fakeObjectsColorsBuffer;
	SingleBuffer realObjectsVertexBuffer;
	SingleBuffer realObjectsIndicesBuffer;
	SingleBuffer constantBuffer;
};

struct GpuComputeResources
{
	ID3D11ShaderResourceView* fakeObjectsInputResourceView;
	ID3D11ShaderResourceView* realObjectsInputResourceView;
	ID3D11UnorderedAccessView* outputResourceView;
	ID3D11Buffer* fakeObjectsInputGpuBuffer;
	ID3D11Buffer* realObjectsInputGpuBuffer;
	ID3D11Buffer* outputGpuBuffer;
};

struct DirectX11State
{
	ID3D11Device* device;
	IDXGIOutput* outputTarget;
	ID3D11DeviceContext* deviceContext;
	IDXGISwapChain* swapChain;
	ID3D11RenderTargetView* renderTargetView;
	ID3D11DepthStencilView* depthStencilView;
	ID3D11RasterizerState* rasterizerState;
	ID3D11DepthStencilState* depthStencilState;
	ID3D11SamplerState* samplerState;
	ID3D11Texture2D* backBuffer;
	ID3D11Texture2D* depthStencilBuffer;
};

struct FontData
{
	DirectX::SpriteBatch* spriteBatch;
	DirectX::SpriteFont* spriteFont;
};

struct WindowData
{
	DirectX11State directX11State;
	GpuComputeResources gpuComputeResources;
	FontData fontData;
	UINT windowWidth;
	UINT windowHeight;
	BuffersList buffersList;
	UINT fakeElementsAmount;
	UINT realElementsAmount;
	RealObjectVertexPrimitive* realVertexPrimitives;
	ShadersList shadersList;
	std::map<std::string, ID3D11ShaderResourceView*> texturesList;
};

void ShowIfError(HRESULT hr, std::wstring customMessage)
{
	if (FAILED(hr))
	{
		_com_error err(hr); // wide strings require L before the string literal
		std::wstring error_message = L"ERROR: " + customMessage + L"\n" + err.ErrorMessage();
		MessageBoxW(NULL, error_message.c_str(), L"Error", MB_ICONERROR);
	}
}

void AllocateInputShaderData(
	ID3D11Device* device,
	void* pData,
	UINT strideLength,
	UINT dataLength,
	ID3D11ShaderResourceView** ppResourceViewToCreate,
	ID3D11Buffer** ppGpuBuffer)
{
	D3D11_BUFFER_DESC gpuBufferDesc = {};

	//gpuBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	//gpuBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE | D3D11_CPU_ACCESS_READ;
	gpuBufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	gpuBufferDesc.ByteWidth = dataLength;
	gpuBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED; // this shit was causing the issue.
	gpuBufferDesc.StructureByteStride = strideLength; // 4 for RGBA

	D3D11_SUBRESOURCE_DATA subData = {};
	subData.pSysMem = pData;
	HRESULT hr = device->CreateBuffer(&gpuBufferDesc, &subData, ppGpuBuffer);

	ShowIfError(hr, L"Input compute shader buffer init error");

	D3D11_SHADER_RESOURCE_VIEW_DESC resourceViewDesc = {};
	resourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
	resourceViewDesc.BufferEx.FirstElement = 0;
	resourceViewDesc.Format = DXGI_FORMAT_UNKNOWN;
	resourceViewDesc.BufferEx.NumElements = dataLength / strideLength;

	hr = device->CreateShaderResourceView(*ppGpuBuffer, &resourceViewDesc, ppResourceViewToCreate);
	ShowIfError(hr, L"Input compute shader resource view init error");
}

void AllocateOutputShaderData(
	ID3D11Device* device,
	UINT strideLength,
	UINT dataLength,
	ID3D11UnorderedAccessView** ppResourceViewToRead,
	ID3D11Buffer** pGpuBuffer)
{
	D3D11_BUFFER_DESC ouputBufferDesc = {};
	ouputBufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	ouputBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	ouputBufferDesc.ByteWidth = dataLength;
	ouputBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	ouputBufferDesc.StructureByteStride = strideLength; // 4 for RGBA
	HRESULT hr = device->CreateBuffer(&ouputBufferDesc, NULL, pGpuBuffer);

	ShowIfError(hr, L"Output compute shader buffer init error");

	D3D11_UNORDERED_ACCESS_VIEW_DESC resourceViewDesc = {};
	resourceViewDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	resourceViewDesc.Buffer.FirstElement = 0;
	resourceViewDesc.Format = DXGI_FORMAT_UNKNOWN;
	resourceViewDesc.Buffer.NumElements = dataLength / strideLength;
	hr = device->CreateUnorderedAccessView(*pGpuBuffer, &resourceViewDesc, ppResourceViewToRead);
	ShowIfError(hr, L"Output compute shader UAV init error");
}

IDXGIAdapter* GetRightAdapter()
{
	std::vector<IDXGIAdapter*> adapters;
	std::vector<DXGI_ADAPTER_DESC> adaptersDescriptions;
	IDXGIFactory* pFactory = nullptr;
	HRESULT hr = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&pFactory);

	ShowIfError(hr, L"DXGI Factory init error");

	IDXGIAdapter* pAdapter;
	DXGI_ADAPTER_DESC pAdapterDescription;
	UINT index = 0;
	while (SUCCEEDED(pFactory->EnumAdapters(index, &pAdapter)))
	{
		hr = pAdapter->GetDesc(&pAdapterDescription);
		ShowIfError(hr, L"Reading adapter desc error");

		adapters.push_back(pAdapter);
		adaptersDescriptions.push_back(pAdapterDescription);
		index++;
	}

	UINT maxMemoryAdapterIndex = 0;
	SIZE_T maxMemory = 0;

	for (UINT i = 0; i < adapters.size(); i++)
	{
		if (adaptersDescriptions[i].DedicatedVideoMemory > maxMemory)
		{
			maxMemory = adaptersDescriptions[i].DedicatedVideoMemory;
			maxMemoryAdapterIndex = i;
		}
	}

	return adapters[maxMemoryAdapterIndex];
}

void InitDeviceAndSwapChain(HWND hwnd, IDXGIAdapter* adapter, UINT windowWidth, UINT windowHeight, DirectX11State* directX11State)
{
	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
	swapChainDesc.BufferDesc.Width = windowWidth;
	swapChainDesc.BufferDesc.Height = windowHeight;
	swapChainDesc.BufferDesc.RefreshRate.Numerator = 40;
	swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	swapChainDesc.SampleDesc.Count = 1; // whats that?
	swapChainDesc.SampleDesc.Quality = 0;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = 1;
	swapChainDesc.OutputWindow = hwnd;
	swapChainDesc.Windowed = TRUE;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	UINT flags = 0;
#ifdef _DEBUG
	flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	HRESULT hr = D3D11CreateDeviceAndSwapChain(
		adapter,
		D3D_DRIVER_TYPE_UNKNOWN,
		NULL,
		flags,
		NULL,
		0,
		D3D11_SDK_VERSION,
		&swapChainDesc,
		&directX11State->swapChain,
		&directX11State->device,
		NULL,
		&directX11State->deviceContext
	);
	ShowIfError(hr, L"D3D11 init device and swapchain");

	adapter->Release();
}

void CreateDirectX11Resources(UINT windowWidth, UINT windowHeight, DirectX11State* directX11State)
{
	HRESULT hr = directX11State->swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&directX11State->backBuffer);
	ShowIfError(hr, L"Getting back buffer error");

	hr = directX11State->device->CreateRenderTargetView(directX11State->backBuffer, NULL, &directX11State->renderTargetView);
	ShowIfError(hr, L"Render target init error");

	//depth/stencil buffers
	D3D11_TEXTURE2D_DESC depthStencilTextureDesc = {};
	depthStencilTextureDesc.Width = windowWidth;
	depthStencilTextureDesc.Height = windowHeight;
	depthStencilTextureDesc.MipLevels = 1;
	depthStencilTextureDesc.ArraySize = 1;
	depthStencilTextureDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthStencilTextureDesc.SampleDesc.Count = 1;
	depthStencilTextureDesc.SampleDesc.Quality = 0;
	depthStencilTextureDesc.Usage = D3D11_USAGE_DEFAULT;
	depthStencilTextureDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	depthStencilTextureDesc.CPUAccessFlags = 0;
	depthStencilTextureDesc.MiscFlags = 0;

	hr = directX11State->device->CreateTexture2D(&depthStencilTextureDesc, NULL, &directX11State->depthStencilBuffer);
	ShowIfError(hr, L"2D texture init error");

	hr = directX11State->device->CreateDepthStencilView(directX11State->depthStencilBuffer, NULL, &directX11State->depthStencilView);
	ShowIfError(hr, L"DepthStencilView init error");

	directX11State->deviceContext->OMSetRenderTargets(1, &directX11State->renderTargetView, directX11State->depthStencilView);

	D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
	depthStencilDesc.DepthEnable = true;
	depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK::D3D11_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D11_COMPARISON_FUNC::D3D11_COMPARISON_LESS_EQUAL;
	hr = directX11State->device->CreateDepthStencilState(&depthStencilDesc, &directX11State->depthStencilState);
	ShowIfError(hr, L"DepthStencilView State init error");

	D3D11_VIEWPORT viewport = {};
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.Width = (FLOAT)windowWidth;
	viewport.Height = (FLOAT)windowHeight;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	directX11State->deviceContext->RSSetViewports(1, &viewport);

	D3D11_RASTERIZER_DESC rasteriserDesc = {};
	rasteriserDesc.FillMode = D3D11_FILL_MODE::D3D11_FILL_SOLID;
	rasteriserDesc.CullMode = D3D11_CULL_MODE::D3D11_CULL_BACK;
	rasteriserDesc.MultisampleEnable = true;
	//rasteriserDesc.AntialiasedLineEnable = true;
	hr = directX11State->device->CreateRasterizerState(&rasteriserDesc, &directX11State->rasterizerState);
	ShowIfError(hr, L"Resterizer state init error");

	D3D11_SAMPLER_DESC samplerDesc = {};
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	samplerDesc.MinLOD = 0;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
	hr = directX11State->device->CreateSamplerState(&samplerDesc, &directX11State->samplerState);
	ShowIfError(hr, L"Sampler state init error");
}

DirectX11State InitDirectX11(HWND hwnd, IDXGIAdapter* adapter, UINT windowWidth, UINT windowHeight)
{
	DirectX11State directX11State = {};

	InitDeviceAndSwapChain(hwnd, adapter, windowWidth, windowHeight, &directX11State);

	CreateDirectX11Resources(windowWidth, windowHeight, &directX11State);

	return directX11State;
}

ComputeShader LoadComputeShader(ID3D11Device* device, std::wstring shaderPath)
{
	ComputeShader computeShader = {};
	ID3D10Blob* shaderBuffer;
	HRESULT hr = D3DReadFileToBlob(shaderPath.c_str(), &shaderBuffer);
	ShowIfError(hr, L"Compute shader file reading error");

	hr = device->CreateComputeShader(
		shaderBuffer->GetBufferPointer(),
		shaderBuffer->GetBufferSize(),
		NULL,
		&computeShader.shader
	);
	ShowIfError(hr, L"Compute shader init error");

	return computeShader;
}

ID3D11VertexShader* LoadVertexShader(ID3D11Device* device, std::wstring shaderPath, ID3D10Blob** ppShaderBuffer)
{
	ID3D11VertexShader* vertexShader;
	HRESULT hr = D3DReadFileToBlob(shaderPath.c_str(), ppShaderBuffer);
	ShowIfError(hr, L"Vertex shader file reading error");

	hr = device->CreateVertexShader(
		(*ppShaderBuffer)->GetBufferPointer(),
		(*ppShaderBuffer)->GetBufferSize(),
		NULL,
		&vertexShader
	);
	ShowIfError(hr, L"Vertex shader init error");

	return vertexShader;
}

PixelShader LoadPixelShader(ID3D11Device* device, std::wstring shaderPath)
{
	PixelShader pixelShader = {};
	ID3D10Blob* shaderBuffer = nullptr;
	HRESULT hr = D3DReadFileToBlob(shaderPath.c_str(), &shaderBuffer);
	ShowIfError(hr, L"Pixel shader file reading error");

	hr = device->CreatePixelShader(
		shaderBuffer->GetBufferPointer(),
		shaderBuffer->GetBufferSize(),
		NULL,
		&pixelShader.shader
	);
	ShowIfError(hr, L"Pixel shader init error");

	return pixelShader;
}

GeometryShader LoadGeometryShader(ID3D11Device* device, std::wstring shaderPath)
{
	GeometryShader geometryShader = {};
	ID3D10Blob* shaderSourceBuffer = nullptr;
	HRESULT hr = D3DReadFileToBlob(shaderPath.c_str(), &shaderSourceBuffer);
	ShowIfError(hr, L"Geometry shader file reading error");

	hr = device->CreateGeometryShader(
		shaderSourceBuffer->GetBufferPointer(),
		shaderSourceBuffer->GetBufferSize(),
		NULL,
		&geometryShader.shader
	);
	ShowIfError(hr, L"Geometry shader init error");

	return geometryShader;
}

ShadersList InitShaders(ID3D11Device* device)
{
	std::wstring shaderFolder = L""; // default value if it's compiled not in Visual Studio
#pragma region DetermineShaderPath
	if (IsDebuggerPresent() == TRUE)
	{
#ifdef _DEBUG
#ifdef _WIN64
		shaderFolder = L"../x64/Debug/";
#else // (win32)
		shaderFolder = L"../Debug/";
#endif
#else
#ifdef _WIN64
		shaderFolder = L"../x64/Release/";
#else // (win32)
		shaderFolder = L"../Release/";
#endif
#endif
	}
	D3D11_INPUT_ELEMENT_DESC fakeLayoutDesc[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_CLASSIFICATION::D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_CLASSIFICATION::D3D11_INPUT_PER_VERTEX_DATA, 0 },
		//{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_CLASSIFICATION::D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};
	std::map<std::string, VertexShader> vertexShaders;
	std::map<std::string, PixelShader> pixelShaders;

	ID3D10Blob* shaderBuffer = nullptr;
	ShadersList shadersList = {};

	// fake objects shaders
	shadersList.geometryShaders["default"] = LoadGeometryShader(device, shaderFolder + L"geometryshader.cso");
	shadersList.computeShaders["default"] = LoadComputeShader(device, shaderFolder + L"computeshader.cso");

	shadersList.pixelShaders["default"] = LoadPixelShader(device, shaderFolder + L"pixelshader.cso");
	shadersList.vertexShaders["default"].shader = LoadVertexShader(device, shaderFolder + L"vertexshader.cso", &shaderBuffer);
	HRESULT hr = device->CreateInputLayout(
		fakeLayoutDesc,
		ARRAYSIZE(fakeLayoutDesc),
		shaderBuffer->GetBufferPointer(),
		shaderBuffer->GetBufferSize(),
		&shadersList.vertexShaders["default"].inputLayout
	);
	ShowIfError(hr, L"Input real layout init error");

	// real objects shaders
	D3D11_INPUT_ELEMENT_DESC realLayoutDesc[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_CLASSIFICATION::D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_CLASSIFICATION::D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};

	shadersList.pixelShaders["real"] = LoadPixelShader(device, shaderFolder + L"realObjectsPixelshader.cso");
	shadersList.vertexShaders["real"].shader = LoadVertexShader(device, shaderFolder + L"realObjectsVertexshader.cso", &shaderBuffer);
	hr = device->CreateInputLayout(
		realLayoutDesc,
		ARRAYSIZE(realLayoutDesc),
		shaderBuffer->GetBufferPointer(),
		shaderBuffer->GetBufferSize(),
		&shadersList.vertexShaders["real"].inputLayout
	);
	ShowIfError(hr, L"Input fake layout init error");
	return shadersList;
}

BuffersList InitScene(ID3D11Device* device,
	DirectX::XMFLOAT3* fakeObjectsPositions, DirectX::XMFLOAT3* fakeObjectsColors, UINT fakeObjectsAmount,
	RealObjectVertexPrimitive* realObjectsVertices, UINT realObjectsAmount)
{
	BuffersList buffersList = {};

	// create fake objects positions buffer for gpu
	D3D11_BUFFER_DESC fakePositionsBufferDesc = {};
	fakePositionsBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	fakePositionsBufferDesc.ByteWidth = sizeof(DirectX::XMFLOAT3) * fakeObjectsAmount;
	fakePositionsBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	fakePositionsBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	fakePositionsBufferDesc.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA fakePositionsBufferData = {};
	fakePositionsBufferData.pSysMem = fakeObjectsPositions;

	buffersList.fakeObjectsPositionsBuffer.length = fakeObjectsAmount;
	buffersList.fakeObjectsPositionsBuffer.strideLength = sizeof(DirectX::XMFLOAT3);
	HRESULT hr = device->CreateBuffer(&fakePositionsBufferDesc, &fakePositionsBufferData, &buffersList.fakeObjectsPositionsBuffer.buffer);
	ShowIfError(hr, L"Fake vertices positions buffer init error");

	// create fake objects colors buffer for gpu
	D3D11_BUFFER_DESC fakeColorsBufferDesc = {};
	fakeColorsBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	fakeColorsBufferDesc.ByteWidth = sizeof(DirectX::XMFLOAT3) * fakeObjectsAmount;
	fakeColorsBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	fakeColorsBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	fakeColorsBufferDesc.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA fakeColorsBufferData = {};
	fakeColorsBufferData.pSysMem = fakeObjectsColors;

	buffersList.fakeObjectsColorsBuffer.length = fakeObjectsAmount;
	buffersList.fakeObjectsColorsBuffer.strideLength = sizeof(DirectX::XMFLOAT3);
	hr = device->CreateBuffer(&fakeColorsBufferDesc, &fakeColorsBufferData, &buffersList.fakeObjectsColorsBuffer.buffer);
	ShowIfError(hr, L"Fake vertices colors buffer init error");

	// create real objects vertex buffer
	UINT singleRealVertexSize = sizeof(RealObjectVertexPrimitive);

	D3D11_BUFFER_DESC realVertexBufferDesc = {};
	realVertexBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	realVertexBufferDesc.ByteWidth = singleRealVertexSize * realObjectsAmount * 8; // cube has 8 vertices
	realVertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	realVertexBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	realVertexBufferDesc.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA realVertexBufferData = {};
	realVertexBufferData.pSysMem = realObjectsVertices;

	buffersList.realObjectsVertexBuffer.length = realObjectsAmount * 8;
	buffersList.realObjectsVertexBuffer.strideLength = singleRealVertexSize;
	hr = device->CreateBuffer(&realVertexBufferDesc, &realVertexBufferData, &buffersList.realObjectsVertexBuffer.buffer);
	ShowIfError(hr, L"Real vertices buffer init error");

	// create indices buffer for real objects
	DWORD* indices = (DWORD*)malloc(sizeof(DWORD) * 36 * realObjectsAmount);

	for (UINT i = 0; i < realObjectsAmount; i++)
	{
		UINT vertexI = i * 8;
		UINT indexI = i * 36;

		// front side
		indices[indexI + 0] = vertexI + 0;
		indices[indexI + 1] = vertexI + 1;
		indices[indexI + 2] = vertexI + 2;
		indices[indexI + 3] = vertexI + 2;
		indices[indexI + 4] = vertexI + 3;
		indices[indexI + 5] = vertexI + 0;

		// top side
		indices[indexI + 6] = vertexI + 0;
		indices[indexI + 7] = vertexI + 4;
		indices[indexI + 8] = vertexI + 5;
		indices[indexI + 9] = vertexI + 5;
		indices[indexI + 10] = vertexI + 1;
		indices[indexI + 11] = vertexI + 0;

		// bottom side
		indices[indexI + 12] = vertexI + 3;
		indices[indexI + 13] = vertexI + 2;
		indices[indexI + 14] = vertexI + 7;
		indices[indexI + 15] = vertexI + 2;
		indices[indexI + 16] = vertexI + 6;
		indices[indexI + 17] = vertexI + 7;

		// left side
		indices[indexI + 18] = vertexI + 4;
		indices[indexI + 19] = vertexI + 0;
		indices[indexI + 20] = vertexI + 3;
		indices[indexI + 21] = vertexI + 4;
		indices[indexI + 22] = vertexI + 3;
		indices[indexI + 23] = vertexI + 7;

		// right side
		indices[indexI + 24] = vertexI + 2;
		indices[indexI + 25] = vertexI + 1;
		indices[indexI + 26] = vertexI + 5;
		indices[indexI + 27] = vertexI + 2;
		indices[indexI + 28] = vertexI + 5;
		indices[indexI + 29] = vertexI + 6;

		// back side
		indices[indexI + 30] = vertexI + 6;
		indices[indexI + 31] = vertexI + 5;
		indices[indexI + 32] = vertexI + 4;
		indices[indexI + 33] = vertexI + 4;
		indices[indexI + 34] = vertexI + 7;
		indices[indexI + 35] = vertexI + 6;
	}

	UINT indicesBufferSize = 36 * realObjectsAmount;
	D3D11_BUFFER_DESC indicesBufferDesc = {};
	indicesBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	indicesBufferDesc.ByteWidth = sizeof(DWORD) * indicesBufferSize;
	indicesBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	indicesBufferDesc.CPUAccessFlags = 0;
	indicesBufferDesc.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA indicesBufferData = {};
	indicesBufferData.pSysMem = indices;

	buffersList.realObjectsIndicesBuffer.length = indicesBufferSize;
	buffersList.realObjectsIndicesBuffer.strideLength = sizeof(DWORD);
	hr = device->CreateBuffer(&indicesBufferDesc, &indicesBufferData, &buffersList.realObjectsIndicesBuffer.buffer);
	ShowIfError(hr, L"Real indices buffer init error");

	// constant buffer init
	D3D11_BUFFER_DESC constantBufferDesc = {};
	constantBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	constantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	constantBufferDesc.MiscFlags = 0;
	constantBufferDesc.ByteWidth = (sizeof(CB_VS_vertexshader) + (16 - sizeof(CB_VS_vertexshader) % 16));
	constantBufferDesc.StructureByteStride = 0;
	hr = device->CreateBuffer(&constantBufferDesc, 0, &buffersList.constantBuffer.buffer);
	ShowIfError(hr, L"Constant buffer init error");

	return buffersList;
}

void RunComputeShader(
	ID3D11Device* device,
	ID3D11DeviceContext* deviceContext,
	ComputeShader computeShader,
	std::map<UINT, ID3D11ShaderResourceView*> shaderResourceViewsMap,
	std::map<UINT, ID3D11UnorderedAccessView*> unorderedAccessViewsMap,
	ID3D11Buffer* outputGpuBuffer, // create output buffer struct that will hold data output data from CS
	void* outputCpuBuffer
)
{
	deviceContext->CSSetShader(computeShader.shader, NULL, 0);

	for (auto it = shaderResourceViewsMap.begin(); it != shaderResourceViewsMap.end(); it++)
	{
		deviceContext->CSSetShaderResources(it->first, 1, &it->second);
	}

	for (auto it = unorderedAccessViewsMap.begin(); it != unorderedAccessViewsMap.end(); it++)
	{
		deviceContext->CSSetUnorderedAccessViews(it->first, 1, &it->second, 0);
	}

	deviceContext->Dispatch(400, 10, 1);

	D3D11_BUFFER_DESC outputBufferDesc = {};
	outputGpuBuffer->GetDesc(&outputBufferDesc);
	outputBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	outputBufferDesc.Usage = D3D11_USAGE_STAGING;
	outputBufferDesc.BindFlags = 0;
	outputBufferDesc.MiscFlags = 0;

	D3D11_MAPPED_SUBRESOURCE mappedSubresource = {};
	HRESULT hr = deviceContext->Map(outputGpuBuffer, 0, D3D11_MAP_READ, 0, &mappedSubresource);
	ShowIfError(hr, L"Compute shader reading output mapping error");

	memcpy(outputCpuBuffer, mappedSubresource.pData, outputBufferDesc.ByteWidth);
	deviceContext->Unmap(outputGpuBuffer, 0);
}

ID3D11ShaderResourceView* InitTexture(ID3D11Device* device)
{
	ID3D11ShaderResourceView* texture1;
	HRESULT hr = DirectX::CreateWICTextureFromFile(device, L"texture1.jpg", nullptr, &texture1);
	ShowIfError(hr, L"Init Texture");

	return texture1;
}

void RenderFrame(
	DirectX11State directX11State,
	FontData fontData,
	PWSTR fpsCounter,
	ShadersList shadersList,
	std::map<std::string, ID3D11ShaderResourceView*> texturesList,
	BuffersList buffersList)
{
	FLOAT bgColor[] = { 0.0f, 0.0f, 0.0f, 0.1f };

	directX11State.deviceContext->ClearRenderTargetView(directX11State.renderTargetView, bgColor);
	directX11State.deviceContext->ClearDepthStencilView(directX11State.depthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	directX11State.deviceContext->RSSetState(directX11State.rasterizerState);

	directX11State.deviceContext->OMSetDepthStencilState(directX11State.depthStencilState, 0);
	directX11State.deviceContext->PSSetSamplers(0, 1, &directX11State.samplerState);

	directX11State.deviceContext->VSSetConstantBuffers(0, 1, &buffersList.constantBuffer.buffer);
	//> Drawing fake objects
	UINT offset = 0;
	directX11State.deviceContext->VSSetShader(shadersList.vertexShaders["default"].shader, NULL, 0);
	directX11State.deviceContext->PSSetShader(shadersList.pixelShaders["default"].shader, NULL, 0);
	directX11State.deviceContext->GSSetShader(shadersList.geometryShaders["default"].shader, NULL, 0);
	directX11State.deviceContext->IASetInputLayout(shadersList.vertexShaders["default"].inputLayout);
	directX11State.deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY::D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

	ID3D11Buffer* fakeObjectsBuffers[2] = { buffersList.fakeObjectsPositionsBuffer.buffer, buffersList.fakeObjectsColorsBuffer.buffer };
	UINT strides[] = { buffersList.fakeObjectsPositionsBuffer.strideLength, buffersList.fakeObjectsColorsBuffer.strideLength };
	UINT offsets[] = { 0,0 };
	directX11State.deviceContext->IASetVertexBuffers(0, 2, fakeObjectsBuffers, strides, offsets);
	directX11State.deviceContext->Draw(buffersList.fakeObjectsPositionsBuffer.length, 0);

	directX11State.deviceContext->GSSetShader(0, NULL, 0);
	//<

	//> Drawing real objects
	/*
	offset = 0;
	directX11State.deviceContext->VSSetShader(shadersList.vertexShaders["real"].shader, NULL, 0);
	directX11State.deviceContext->PSSetShader(shadersList.pixelShaders["real"].shader, NULL, 0);
	directX11State.deviceContext->IASetInputLayout(shadersList.vertexShaders["real"].inputLayout);
	directX11State.deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY::D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	directX11State.deviceContext->IASetVertexBuffers(0, 1, &buffersList.realObjectsVertexBuffer.buffer, &buffersList.realObjectsVertexBuffer.strideLength, &offset);
	directX11State.deviceContext->PSSetShaderResources(0, 1, &texturesList["default"]);
	directX11State.deviceContext->IASetIndexBuffer(buffersList.realObjectsIndicesBuffer.buffer, DXGI_FORMAT_R32_UINT, 0);
	directX11State.deviceContext->DrawIndexed(buffersList.realObjectsIndicesBuffer.length, 0, 0);
	*/
	//<

	// NOTE: For some reasons if Geometry Shader is set, DirecXTK fonts can't be rendered!
	directX11State.deviceContext->GSSetShader(0, NULL, 0);

	fontData.spriteBatch->Begin();
	fontData.spriteFont->DrawString(fontData.spriteBatch, fpsCounter, DirectX::XMFLOAT2(10.0f, 10.0f), DirectX::Colors::White, 0.0f, DirectX::XMFLOAT2(0.0f, 0.0f), DirectX::XMFLOAT2(1.0f, 1.0f));
	fontData.spriteBatch->End();

	directX11State.swapChain->Present(1, NULL);
}

DirectX::XMFLOAT3 getForce(DirectX::XMFLOAT3 objectAPosition, DirectX::XMFLOAT3 objectBPosition)
{
	DirectX::XMVECTOR dV = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&objectAPosition), DirectX::XMLoadFloat3(&objectBPosition));

	FLOAT distance = DirectX::XMVector3Length(dV).m128_f32[0];

	FLOAT F = 1.0f / (distance * distance + 1);

	DirectX::XMFLOAT3 res;
	DirectX::XMStoreFloat3(&res, DirectX::XMVectorMultiply(dV, { F, F, F }));

	return res;
}

void InitiGameObjects(
	UINT fakeElementsAmount,
	UINT realElementsAmount,
	DirectX::XMFLOAT3* fakeObjectsPositions,
	DirectX::XMFLOAT3* fakeObjectsColors,
	DirectX::XMFLOAT3* fakeObjectsVelocities,
	DirectX::XMFLOAT3* realObjectsPositions,
	DirectX::XMFLOAT3* realObjectsVelocities,
	RealObjectVertexPrimitive* realVertexPrimitives)
{
	FLOAT width = 100.0f;
	FLOAT height = 100.0f;

	INT height2 = 1000;
	FLOAT width2 = (FLOAT)fakeElementsAmount / (FLOAT)height2;

	// generating the sphere with fake objects
	UINT stacksCount = 3200;
	UINT sectorsCount = fakeElementsAmount / stacksCount;
	FLOAT radius = 30.0f;

	for (UINT i = 0; i < sectorsCount; i++)
	{
		FLOAT r = (rand() % 255) / 255.0f;
		FLOAT g = (rand() % 255) / 255.0f;
		FLOAT b = (rand() % 255) / 255.0f;
		for (UINT j = 1; j < stacksCount; j++)
		{
			FLOAT alpha = DirectX::XM_2PI * (FLOAT)i / (FLOAT)sectorsCount;
			FLOAT beta = DirectX::XM_PIDIV2 - DirectX::XM_PI * (FLOAT)j / (FLOAT)stacksCount;

			FLOAT x = (radius * cos(beta)) * cos(alpha);
			FLOAT y = (radius * cos(beta)) * sin(alpha);
			FLOAT z = radius * sin(beta);

			fakeObjectsPositions[j + i * stacksCount] = DirectX::XMFLOAT3(x, y, z);

			alpha += DirectX::XMConvertToRadians(j % 2 == 0 ? 1.0f : -1.0f);
			//beta += DirectX::XMConvertToRadians(1.0f);

			FLOAT xTo = (radius * cos(beta)) * cos(alpha);
			FLOAT yTo = (radius * cos(beta)) * sin(alpha);
			FLOAT zTo = radius * sin(beta);

			fakeObjectsVelocities[j + i * stacksCount] = DirectX::XMFLOAT3(xTo - x, yTo - y, zTo - z);

			fakeObjectsColors[j + i * stacksCount] = DirectX::XMFLOAT3(r, g, b);
		}
	}

	/*for (UINT i = 0; i < fakeElementsAmount; i++)
	{
		FLOAT x = (i % height2) * (width / (FLOAT)height2) - width / 2;
		FLOAT y = ((FLOAT)i / height2) * (height / (FLOAT)width2) - height / 2;

		fakeObjectsPositions[i] = DirectX::XMFLOAT3(x, y, 0.0f);
		fakeObjectsVelocities[i] = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
	}*/

	// randomly generate real objects
	/*for (UINT i = 0; i < realElementsAmount; i++)
	{
		FLOAT x = (FLOAT)(rand() % 10) - 5;
		FLOAT y = (FLOAT)(rand() % 10) - 5;
		FLOAT z = (FLOAT)(rand() % 10) - 5;

		realObjectsPositions[i] = DirectX::XMFLOAT3(x, y, z);
		realObjectsVelocities[i] = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
	}*/

	// generate a single real object in the center
	realObjectsPositions[0] = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
	realObjectsVelocities[0] = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);

	// generate vertices primitives for real objects
	FLOAT cubeScale = 2.0f;
	for (UINT i = 0; i < realElementsAmount; i++)
	{
		UINT j = i * 8;
		// front top left
		realVertexPrimitives[j].position.z = realObjectsPositions[i].z - cubeScale / 2;
		realVertexPrimitives[j].position.x = realObjectsPositions[i].x - cubeScale / 2;
		realVertexPrimitives[j].position.y = realObjectsPositions[i].y + cubeScale / 2;
		realVertexPrimitives[j].texturePosition.x = 0.0f;
		realVertexPrimitives[j].texturePosition.y = 0.0f;

		// front top right
		realVertexPrimitives[j + 1].position.z = realObjectsPositions[i].z - cubeScale / 2;
		realVertexPrimitives[j + 1].position.x = realObjectsPositions[i].x + cubeScale / 2;
		realVertexPrimitives[j + 1].position.y = realObjectsPositions[i].y + cubeScale / 2;
		realVertexPrimitives[j + 1].texturePosition.x = 1.0f;
		realVertexPrimitives[j + 1].texturePosition.y = 0.0f;

		// front bottom right
		realVertexPrimitives[j + 2].position.z = realObjectsPositions[i].z - cubeScale / 2;
		realVertexPrimitives[j + 2].position.x = realObjectsPositions[i].x + cubeScale / 2;
		realVertexPrimitives[j + 2].position.y = realObjectsPositions[i].y - cubeScale / 2;
		realVertexPrimitives[j + 2].texturePosition.x = 1.0f;
		realVertexPrimitives[j + 2].texturePosition.y = 1.0f;

		// front bottom left
		realVertexPrimitives[j + 3].position.z = realObjectsPositions[i].z - cubeScale / 2;
		realVertexPrimitives[j + 3].position.x = realObjectsPositions[i].x - cubeScale / 2;
		realVertexPrimitives[j + 3].position.y = realObjectsPositions[i].y - cubeScale / 2;
		realVertexPrimitives[j + 3].texturePosition.x = 0.0f;
		realVertexPrimitives[j + 3].texturePosition.y = 1.0f;

		// back top left
		realVertexPrimitives[j + 4].position.z = realObjectsPositions[i].z + cubeScale / 2;
		realVertexPrimitives[j + 4].position.x = realObjectsPositions[i].x - cubeScale / 2;
		realVertexPrimitives[j + 4].position.y = realObjectsPositions[i].y + cubeScale / 2;
		realVertexPrimitives[j + 4].texturePosition.x = 1.0f;
		realVertexPrimitives[j + 4].texturePosition.y = 1.0f;

		// back top right
		realVertexPrimitives[j + 5].position.z = realObjectsPositions[i].z + cubeScale / 2;
		realVertexPrimitives[j + 5].position.x = realObjectsPositions[i].x + cubeScale / 2;
		realVertexPrimitives[j + 5].position.y = realObjectsPositions[i].y + cubeScale / 2;
		realVertexPrimitives[j + 5].texturePosition.x = 0.0f;
		realVertexPrimitives[j + 5].texturePosition.y = 1.0f;

		// back bottom right
		realVertexPrimitives[j + 6].position.z = realObjectsPositions[i].z + cubeScale / 2;
		realVertexPrimitives[j + 6].position.x = realObjectsPositions[i].x + cubeScale / 2;
		realVertexPrimitives[j + 6].position.y = realObjectsPositions[i].y - cubeScale / 2;
		realVertexPrimitives[j + 6].texturePosition.x = 0.0f;
		realVertexPrimitives[j + 6].texturePosition.y = 0.0f;

		// back bottom left
		realVertexPrimitives[j + 7].position.z = realObjectsPositions[i].z + cubeScale / 2;
		realVertexPrimitives[j + 7].position.x = realObjectsPositions[i].x - cubeScale / 2;
		realVertexPrimitives[j + 7].position.y = realObjectsPositions[i].y - cubeScale / 2;
		realVertexPrimitives[j + 7].texturePosition.x = 1.0f;
		realVertexPrimitives[j + 7].texturePosition.y = 0.0f;
	}
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_NCCREATE:
	{
		WindowData* pWindowData = (WindowData*)((CREATESTRUCTW*)lParam)->lpCreateParams;
		SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pWindowData);
		break;
	}
	/*case WM_SIZE:
	{
		break;
		WindowData* pWindowData = (WindowData*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

		pWindowData->windowWidth = LOWORD(lParam);
		pWindowData->windowHeight = HIWORD(lParam);

		if (pWindowData->directX11State.swapChain != 0)
		{
			DirectX11State state = pWindowData->directX11State;
			state.deviceContext->ClearState();
			state.deviceContext->Flush();
			state.backBuffer->Release();
			state.depthStencilBuffer->Release();
			state.depthStencilState->Release();
			state.renderTargetView->Release();
			state.depthStencilView->Release();
			//state.depthStencilState->Release();
			state.rasterizerState->Release();
			state.samplerState->Release();
			//state.deviceContext->Release();
			//state.swapChain->Release();
			//state.device->Release();

			pWindowData->buffersList.constantBuffer.buffer->Release();
			pWindowData->buffersList.realObjectsIndicesBuffer.buffer->Release();
			pWindowData->buffersList.fakeObjectsVertexBuffer.buffer->Release();

			for (auto it = pWindowData->texturesList.begin(); it != pWindowData->texturesList.end(); it++)
			{
				it->second->Release();
			}

			for (auto it = pWindowData->shadersList.geometryShaders.begin(); it != pWindowData->shadersList.geometryShaders.end(); it++)
			{
				it->second.shader->Release();
			}

			for (auto it = pWindowData->shadersList.pixelShaders.begin(); it != pWindowData->shadersList.pixelShaders.end(); it++)
			{
				it->second.shader->Release();
			}

			for (auto it = pWindowData->shadersList.vertexShaders.begin(); it != pWindowData->shadersList.vertexShaders.end(); it++)
			{
				it->second.shader->Release();
				it->second.inputLayout->Release();
			}

			//IDXGIDebug* debugDev;
			//HRESULT hr = DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debugDev));

			//debugDev->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
			//wchar_t text_buffer[20] = { 0 }; //temporary buffer
			//swprintf(text_buffer, _countof(text_buffer), L"\n%u\n", pWindowData->windowHeight); // convert
			//OutputDebugString(text_buffer); // print

			pWindowData->directX11State.swapChain->ResizeBuffers(
				1,
				pWindowData->windowWidth,
				pWindowData->windowHeight,
				DXGI_FORMAT_R8G8B8A8_UNORM,
				DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH
			);

			CreateDirectX11Resources(pWindowData->windowWidth, pWindowData->windowHeight, &pWindowData->directX11State);
			pWindowData->buffersList = InitScene(pWindowData->directX11State.device, pWindowData->vertexPrimitives, pWindowData->fakeElementsAmount);
			pWindowData->shadersList = InitShaders(pWindowData->directX11State.device);
			pWindowData->texturesList["default"] = InitTexture(pWindowData->directX11State.device);
		}
		break;
	}*/
	case WM_KEYDOWN:
	{
		keysPressed[wParam] = true;
		break;
	}
	case WM_KEYUP:
	{
		keysPressed[wParam] = false;

		if (wParam == VK_ESCAPE)
		{
			DestroyWindow(hwnd);
		}

		break;
	}
	case WM_INPUT:
	{
		UINT dwSize = sizeof(RAWINPUT);
		static BYTE lpb[sizeof(RAWINPUT)] = {};

		GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER));

		RAWINPUT* raw = (RAWINPUT*)lpb;

		if (raw->header.dwType == RIM_TYPEMOUSE)
		{
			int dx = -raw->data.mouse.lLastX;
			int dy = -raw->data.mouse.lLastY;

			yaw += dx * 0.1f;
			pitch += dy * 0.1f;

			if (pitch > 89.0f)
				pitch = 89.0f;
			if (pitch < -89.0f)
				pitch = -89.0f;

			cameraFront = DirectX::XMVector3Normalize({
				(FLOAT)(cos(DirectX::XMConvertToRadians(yaw)) * cos(DirectX::XMConvertToRadians(pitch))),
				(FLOAT)(sin(DirectX::XMConvertToRadians(pitch))),
				(FLOAT)(sin(DirectX::XMConvertToRadians(yaw)) * cos(DirectX::XMConvertToRadians(pitch))),
				});
			//SetCursorPos(500, 500);
		}
		break;
	}
	/*case WM_MOUSEMOVE:
	{
		INT x = LOWORD(lParam);
		INT y = HIWORD(lParam);

		FLOAT dx = mouseX - x;
		FLOAT dy = mouseY - y;

		mouseX = x;
		mouseY = y;

		yaw += dx * 0.1f;
		pitch += dy * 0.1f;

		if (pitch > 89.0f)
			pitch = 89.0f;
		if (pitch < -89.0f)
			pitch = -89.0f;

		cameraFront = DirectX::XMVector3Normalize({
			(FLOAT)(cos(DirectX::XMConvertToRadians(yaw)) * cos(DirectX::XMConvertToRadians(pitch))),
			(FLOAT)(sin(DirectX::XMConvertToRadians(pitch))),
			(FLOAT)(sin(DirectX::XMConvertToRadians(yaw)) * cos(DirectX::XMConvertToRadians(pitch))),
		});
		SetCursorPos(500, 500);

		break;
	}*/
	/*case WM_CLOSE:
	{
		DestroyWindow(hwnd);
		return 0;
	}*/
	case WM_DESTROY:
	{
		PostQuitMessage(0);
		return 0;
	}
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

void ProcessKeyboardEvent()
{
	if (keysPressed[32]) // 32 is space
	{
		stopSimulation = !stopSimulation;
		keysPressed[32] = false;
	}

	if (keysPressed['Q'])
	{
		fakeObjectsSpeed *= 0.90f;

		if (fakeObjectsSpeed < 1.0f)
		{
			fakeObjectsSpeed = 0.1f;
		}
	}

	if (keysPressed['E'])
	{
		fakeObjectsSpeed *= 2.0f;

		if (fakeObjectsSpeed > 10000000.0f)
		{
			fakeObjectsSpeed = 10000000.0f;
		}
	}

	if (keysPressed['R'])
	{
		requestToRestartGame = true;
	}

	if (keysPressed['W'])
	{
		cameraPos = DirectX::XMVectorAdd(cameraPos, cameraFront);
	}

	if (keysPressed['S'])
	{
		cameraPos = DirectX::XMVectorSubtract(cameraPos, cameraFront);
	}

	if (keysPressed['D'])
	{
		cameraPos = DirectX::XMVectorSubtract(cameraPos, DirectX::XMVector3Normalize(DirectX::XMVector3Cross(cameraFront, cameraUp)));
	}

	if (keysPressed['A'])
	{
		cameraPos = DirectX::XMVectorAdd(cameraPos, DirectX::XMVector3Normalize(DirectX::XMVector3Cross(cameraFront, cameraUp)));
	}
}

int WINAPI wWinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR lpCmdLine,
	_In_ int nShowCmd
)
{
	WNDCLASS wc = {};

	wchar_t className[] = L"n body";

	wc.lpfnWndProc = WndProc;
	wc.lpszClassName = className;
	wc.hInstance = hInstance;

	RegisterClass(&wc);

	WindowData windowData = {};

	windowData.fakeElementsAmount = 256000;
	windowData.realElementsAmount = 1;

	HWND hwnd = CreateWindowEx(
		0,
		className,
		L"n body",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		0,
		0,
		hInstance,
		&windowData
	);

	if (hwnd == 0)
	{
		return 1;
	}

	ShowWindow(hwnd, SW_SHOWMAXIMIZED);

	// register input device
	RAWINPUTDEVICE Rid[1] = {};
	Rid[0].usUsagePage = HID_USAGE_PAGE_GENERIC;
	Rid[0].usUsage = HID_USAGE_GENERIC_MOUSE;
	Rid[0].dwFlags = RIDEV_INPUTSINK;
	Rid[0].hwndTarget = hwnd;
	RegisterRawInputDevices(Rid, 1, sizeof(Rid[0]));

	//> keep cursor in the window
	RECT rect;
	GetClientRect(hwnd, &rect);

	POINT ul = {};
	ul.x = rect.left;
	ul.y = rect.top;

	POINT lr = {};
	lr.x = rect.right;
	lr.y = rect.bottom;

	MapWindowPoints(hwnd, nullptr, &ul, 1);
	MapWindowPoints(hwnd, nullptr, &lr, 1);

	rect.left = ul.x;
	rect.top = ul.y;

	rect.right = lr.x;
	rect.bottom = lr.y;
	ClipCursor(&rect);
	//<
	ShowCursor(FALSE);

	srand((UINT)time(NULL));

	LARGE_INTEGER freq;
	QueryPerformanceFrequency(&freq);

	IDXGIAdapter* adapter = GetRightAdapter();

	UINT i = 0;
	IDXGIOutput* outputTarget;
	// it's possible to have multiple output devices, but for now it's ignored
	while (adapter->EnumOutputs(i, &outputTarget) == S_OK)
	{
		DXGI_OUTPUT_DESC desc;
		outputTarget->GetDesc(&desc);
		windowData.directX11State.outputTarget = outputTarget;

		windowData.windowWidth = desc.DesktopCoordinates.right;
		windowData.windowHeight = desc.DesktopCoordinates.bottom;
		i++;
	}

	windowData.directX11State = InitDirectX11(hwnd, adapter, windowData.windowWidth, windowData.windowHeight);

	FLOAT bgColor[] = { 0.0f, 0.0f, 0.0f, 0.1f };

	windowData.directX11State.swapChain->SetFullscreenState(true, 0);

	windowData.shadersList = InitShaders(windowData.directX11State.device);

	windowData.texturesList["default"] = InitTexture(windowData.directX11State.device);

	UINT fakeObjectPositionsMemSize = sizeof(DirectX::XMFLOAT3) * windowData.fakeElementsAmount;
	UINT fakeObjectVelocitiesMemSize = sizeof(DirectX::XMFLOAT3) * windowData.fakeElementsAmount;
	UINT fakeObjectColorsMemSize = sizeof(DirectX::XMFLOAT3) * windowData.fakeElementsAmount;

	UINT realObjectPositionsMemSize = sizeof(DirectX::XMFLOAT3) * windowData.realElementsAmount;
	UINT realObjectVelocitiesMemSize = sizeof(DirectX::XMFLOAT3) * windowData.realElementsAmount;

	//windowData.fakeVertexPrimitives = (FakeObjectVertexPrimitive*)malloc(sizeof(FakeObjectVertexPrimitive) * windowData.fakeElementsAmount);
	windowData.realVertexPrimitives = (RealObjectVertexPrimitive*)malloc(sizeof(RealObjectVertexPrimitive) * windowData.realElementsAmount * 8);
	DirectX::XMFLOAT3* fakeObjectPositions = (DirectX::XMFLOAT3*)malloc(fakeObjectPositionsMemSize);
	DirectX::XMFLOAT3* fakeObjectVelocities = (DirectX::XMFLOAT3*)malloc(fakeObjectVelocitiesMemSize);
	DirectX::XMFLOAT3* fakeObjectColors = (DirectX::XMFLOAT3*)malloc(fakeObjectColorsMemSize);
	DirectX::XMFLOAT3* realObjectPositions = (DirectX::XMFLOAT3*)malloc(realObjectPositionsMemSize);
	DirectX::XMFLOAT3* realObjectVelocities = (DirectX::XMFLOAT3*)malloc(realObjectVelocitiesMemSize);

	InitiGameObjects(windowData.fakeElementsAmount, windowData.realElementsAmount,
		fakeObjectPositions, fakeObjectColors, fakeObjectVelocities,
		realObjectPositions, realObjectVelocities, windowData.realVertexPrimitives);

	windowData.buffersList = InitScene(windowData.directX11State.device, fakeObjectPositions, fakeObjectColors,
		windowData.fakeElementsAmount, windowData.realVertexPrimitives, windowData.realElementsAmount);

	// init font's data
	windowData.fontData.spriteBatch = new DirectX::SpriteBatch(windowData.directX11State.deviceContext);
	windowData.fontData.spriteFont = new DirectX::SpriteFont(windowData.directX11State.device, L"default.spritefont");

	CB_VS_vertexshader constantBuffer = {};
	constantBuffer.projectionMatrix = DirectX::XMMatrixTranspose(DirectX::XMMatrixPerspectiveFovLH((45.0f / 360.0f) * DirectX::XM_2PI, (FLOAT)windowData.windowWidth / (FLOAT)windowData.windowHeight, 0.1f, 100.0f));

	ID3D11ShaderResourceView* fakeObjectsInputResourceView;
	ID3D11ShaderResourceView* realObjectsInputResourceView;
	ID3D11UnorderedAccessView* outputResourceView;
	ID3D11Buffer* fakeObjectsInputGpuBuffer;
	ID3D11Buffer* realObjectsInputGpuBuffer;
	ID3D11Buffer* outputGpuBuffer;
	AllocateInputShaderData(windowData.directX11State.device, fakeObjectPositions, sizeof(DirectX::XMFLOAT3), fakeObjectPositionsMemSize, &fakeObjectsInputResourceView, &fakeObjectsInputGpuBuffer);
	AllocateInputShaderData(windowData.directX11State.device, realObjectPositions, sizeof(DirectX::XMFLOAT3), realObjectPositionsMemSize, &realObjectsInputResourceView, &realObjectsInputGpuBuffer);
	AllocateOutputShaderData(windowData.directX11State.device, sizeof(DirectX::XMFLOAT3), fakeObjectVelocitiesMemSize, &outputResourceView, &outputGpuBuffer);

	D3D11_BUFFER_DESC outputGpuBufferDesc = {};
	outputGpuBuffer->GetDesc(&outputGpuBufferDesc);
	DirectX::XMFLOAT3* outputCpuBuffer = (DirectX::XMFLOAT3*)malloc(outputGpuBufferDesc.ByteWidth);

	MSG msg = {};
	LARGE_INTEGER lastTimestamp;
	FLOAT deltaTimestamp = 0.0f;
	wchar_t deltaTimestampTextBuffer[20] = { 0 };

	QueryPerformanceCounter(&lastTimestamp);
	while (true)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT) // quit the loop
			{
				break;
			}

			TranslateMessage(&msg);
			DispatchMessage(&msg);
			continue;
		}

		ProcessKeyboardEvent();

		constantBuffer.viewMatrix = DirectX::XMMatrixTranspose(DirectX::XMMatrixLookAtLH(
			cameraPos,
			DirectX::XMVectorAdd(cameraPos, cameraFront),
			DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)
		));
		// set constant buffer
		D3D11_MAPPED_SUBRESOURCE constantBufferSubresource = {};
		windowData.directX11State.deviceContext->Map(windowData.buffersList.constantBuffer.buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &constantBufferSubresource);
		CopyMemory(constantBufferSubresource.pData, &constantBuffer, sizeof(CB_VS_vertexshader));
		windowData.directX11State.deviceContext->Unmap(windowData.buffersList.constantBuffer.buffer, 0);

		std::map<UINT, ID3D11ShaderResourceView*> shaderResourceViews =
		{
			{ 0, fakeObjectsInputResourceView },
			{ 1, realObjectsInputResourceView }
		};

		std::map<UINT, ID3D11UnorderedAccessView*> unorderedAccessViews =
		{
			{ 0, outputResourceView }
		};

		if (!stopSimulation)
		{
			RunComputeShader(
				windowData.directX11State.device,
				windowData.directX11State.deviceContext,
				windowData.shadersList.computeShaders["default"],
				shaderResourceViews,
				unorderedAccessViews,
				outputGpuBuffer,
				outputCpuBuffer);
			FLOAT cubeScale = 2.0f;
			// update real objects
			for (UINT i = 0; i < windowData.realElementsAmount; i++) {
				DirectX::XMFLOAT3 totalForce = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);

				for (UINT j = 0; j < windowData.realElementsAmount; j++) {
					if (i == j) continue;

					DirectX::XMFLOAT3 force = getForce(realObjectPositions[j], realObjectPositions[i]);

					DirectX::XMVECTOR test = DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&totalForce), DirectX::XMLoadFloat3(&force));

					DirectX::XMStoreFloat3(&totalForce, test);
				}
				totalForce = getForce(DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f), realObjectPositions[i]);

				realObjectVelocities[i].x += totalForce.x / (windowData.realElementsAmount * 1);
				realObjectVelocities[i].y += totalForce.y / (windowData.realElementsAmount * 1);
				realObjectVelocities[i].z += totalForce.z / (windowData.realElementsAmount * 1);

				realObjectPositions[i].x += realObjectVelocities[i].x;
				realObjectPositions[i].y += realObjectVelocities[i].y;
				realObjectPositions[i].z += realObjectVelocities[i].y;

				UINT j = i * 8;
				// front left right
				windowData.realVertexPrimitives[j].position.z = realObjectPositions[i].z - cubeScale / 2;
				windowData.realVertexPrimitives[j].position.x = realObjectPositions[i].x - cubeScale / 2;
				windowData.realVertexPrimitives[j].position.y = realObjectPositions[i].y + cubeScale / 2;

				// front top right
				windowData.realVertexPrimitives[j + 1].position.z = realObjectPositions[i].z - cubeScale / 2;
				windowData.realVertexPrimitives[j + 1].position.x = realObjectPositions[i].x + cubeScale / 2;
				windowData.realVertexPrimitives[j + 1].position.y = realObjectPositions[i].y + cubeScale / 2;

				// front bottom right
				windowData.realVertexPrimitives[j + 2].position.z = realObjectPositions[i].z - cubeScale / 2;
				windowData.realVertexPrimitives[j + 2].position.x = realObjectPositions[i].x + cubeScale / 2;
				windowData.realVertexPrimitives[j + 2].position.y = realObjectPositions[i].y - cubeScale / 2;

				// front bottom left
				windowData.realVertexPrimitives[j + 3].position.z = realObjectPositions[i].z - cubeScale / 2;
				windowData.realVertexPrimitives[j + 3].position.x = realObjectPositions[i].x - cubeScale / 2;
				windowData.realVertexPrimitives[j + 3].position.y = realObjectPositions[i].y - cubeScale / 2;

				// back top left
				windowData.realVertexPrimitives[j + 4].position.z = realObjectPositions[i].z + cubeScale / 2;
				windowData.realVertexPrimitives[j + 4].position.x = realObjectPositions[i].x - cubeScale / 2;
				windowData.realVertexPrimitives[j + 4].position.y = realObjectPositions[i].y + cubeScale / 2;

				// back top right
				windowData.realVertexPrimitives[j + 5].position.z = realObjectPositions[i].z + cubeScale / 2;
				windowData.realVertexPrimitives[j + 5].position.x = realObjectPositions[i].x + cubeScale / 2;
				windowData.realVertexPrimitives[j + 5].position.y = realObjectPositions[i].y + cubeScale / 2;

				// back bottom right
				windowData.realVertexPrimitives[j + 6].position.z = realObjectPositions[i].z + cubeScale / 2;
				windowData.realVertexPrimitives[j + 6].position.x = realObjectPositions[i].x + cubeScale / 2;
				windowData.realVertexPrimitives[j + 6].position.y = realObjectPositions[i].y - cubeScale / 2;

				// back bottom left
				windowData.realVertexPrimitives[j + 7].position.z = realObjectPositions[i].z + cubeScale / 2;
				windowData.realVertexPrimitives[j + 7].position.x = realObjectPositions[i].x - cubeScale / 2;
				windowData.realVertexPrimitives[j + 7].position.y = realObjectPositions[i].y - cubeScale / 2;
			}

			// update objects position and velocities from gpu output
			for (UINT i = 0; i < windowData.fakeElementsAmount; i++)
			{
				fakeObjectVelocities[i].x += outputCpuBuffer[i].x / fakeObjectsSpeed;
				fakeObjectVelocities[i].y += outputCpuBuffer[i].y / fakeObjectsSpeed;
				fakeObjectVelocities[i].z += outputCpuBuffer[i].z / fakeObjectsSpeed;
				/*
				fakeObjectVelocities[i].x *= 0.97f;
				fakeObjectVelocities[i].y *= 0.97f;
				fakeObjectVelocities[i].z *= 0.97f;
				*/
				fakeObjectPositions[i].x += fakeObjectVelocities[i].x;
				fakeObjectPositions[i].y += fakeObjectVelocities[i].y;
				fakeObjectPositions[i].z += fakeObjectVelocities[i].z;
			}

			// update fake positions buffer for rendering pipeline
			D3D11_MAPPED_SUBRESOURCE resource = {};
			windowData.directX11State.deviceContext->Map(windowData.buffersList.fakeObjectsPositionsBuffer.buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &resource);
			memcpy(resource.pData, fakeObjectPositions, windowData.fakeElementsAmount * sizeof(DirectX::XMFLOAT3));
			windowData.directX11State.deviceContext->Unmap(windowData.buffersList.fakeObjectsPositionsBuffer.buffer, 0);

			// update real vertex buffer for rendering pipeline
			resource = {};
			windowData.directX11State.deviceContext->Map(windowData.buffersList.realObjectsVertexBuffer.buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &resource);
			memcpy(resource.pData, windowData.realVertexPrimitives, 8 * windowData.realElementsAmount * sizeof(RealObjectVertexPrimitive));
			windowData.directX11State.deviceContext->Unmap(windowData.buffersList.realObjectsVertexBuffer.buffer, 0);

			// update compute buffer for compute shader
			windowData.directX11State.deviceContext->UpdateSubresource(fakeObjectsInputGpuBuffer, 0, 0, fakeObjectPositions, 0, 0);
			windowData.directX11State.deviceContext->UpdateSubresource(realObjectsInputGpuBuffer, 0, 0, realObjectPositions, 0, 0);
		}

		swprintf(deltaTimestampTextBuffer, _countof(deltaTimestampTextBuffer), L"%.0f FPS", 1.0f / deltaTimestamp);
		RenderFrame(windowData.directX11State, windowData.fontData, deltaTimestampTextBuffer, windowData.shadersList, windowData.texturesList, windowData.buffersList);

		if (requestToRestartGame)
		{
			InitiGameObjects(windowData.fakeElementsAmount, windowData.realElementsAmount,
				fakeObjectPositions, fakeObjectColors, fakeObjectVelocities,
				realObjectPositions, realObjectVelocities, windowData.realVertexPrimitives);

			requestToRestartGame = false;
			stopSimulation = true;

			// update fake positions buffer for rendering pipeline
			D3D11_MAPPED_SUBRESOURCE resource = {};
			windowData.directX11State.deviceContext->Map(windowData.buffersList.fakeObjectsPositionsBuffer.buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &resource);
			memcpy(resource.pData, fakeObjectPositions, windowData.fakeElementsAmount * sizeof(DirectX::XMFLOAT3));
			windowData.directX11State.deviceContext->Unmap(windowData.buffersList.fakeObjectsPositionsBuffer.buffer, 0);

			// update real vertex buffer for rendering pipeline
			resource = {};
			windowData.directX11State.deviceContext->Map(windowData.buffersList.realObjectsVertexBuffer.buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &resource);
			memcpy(resource.pData, windowData.realVertexPrimitives, 8 * windowData.realElementsAmount * sizeof(RealObjectVertexPrimitive));
			windowData.directX11State.deviceContext->Unmap(windowData.buffersList.realObjectsVertexBuffer.buffer, 0);
		}

		LARGE_INTEGER currentTimestamp;
		QueryPerformanceCounter(&currentTimestamp);
		deltaTimestamp = (currentTimestamp.QuadPart - lastTimestamp.QuadPart) / (FLOAT)freq.QuadPart;
		lastTimestamp = currentTimestamp;

		//OutputDebugString(text_buffer);
	}

	DirectX11State state = windowData.directX11State;

	// remove full screen before clearing swapchain, otherwise that will some unreleased devices (for uknown reasons)
	state.swapChain->SetFullscreenState(false, 0);

	state.deviceContext->Flush();
	//state.deviceContext->ClearState();
	state.swapChain->Release();
	state.backBuffer->Release();
	state.depthStencilBuffer->Release();
	state.depthStencilState->Release();
	state.renderTargetView->Release();
	state.depthStencilView->Release();
	//state.depthStencilState->Release();
	state.rasterizerState->Release();
	state.samplerState->Release();
	state.device->Release();
	state.deviceContext->Release();

	delete windowData.fontData.spriteBatch;
	delete windowData.fontData.spriteFont;

	fakeObjectsInputResourceView->Release();
	realObjectsInputResourceView->Release();
	outputResourceView->Release();
	fakeObjectsInputGpuBuffer->Release();
	realObjectsInputGpuBuffer->Release();
	outputGpuBuffer->Release();

	windowData.buffersList.constantBuffer.buffer->Release();
	windowData.buffersList.fakeObjectsPositionsBuffer.buffer->Release();
	windowData.buffersList.fakeObjectsColorsBuffer.buffer->Release();
	windowData.buffersList.realObjectsVertexBuffer.buffer->Release();
	windowData.buffersList.realObjectsIndicesBuffer.buffer->Release();

	for (auto it = windowData.texturesList.begin(); it != windowData.texturesList.end(); it++)
	{
		it->second->Release();
	}

	for (auto it = windowData.shadersList.geometryShaders.begin(); it != windowData.shadersList.geometryShaders.end(); it++)
	{
		it->second.shader->Release();
	}

	for (auto it = windowData.shadersList.pixelShaders.begin(); it != windowData.shadersList.pixelShaders.end(); it++)
	{
		it->second.shader->Release();
	}

	for (auto it = windowData.shadersList.vertexShaders.begin(); it != windowData.shadersList.vertexShaders.end(); it++)
	{
		it->second.shader->Release();
		it->second.inputLayout->Release();
	}

	for (auto it = windowData.shadersList.computeShaders.begin(); it != windowData.shadersList.computeShaders.end(); it++)
	{
		it->second.shader->Release();
	}
	free(realObjectPositions);
	free(realObjectVelocities);
	free(fakeObjectPositions);
	free(fakeObjectVelocities);
	//IDXGIDebug* debugDev;
	//HRESULT hr = DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debugDev));
	//hr = debugDev->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);

	return 0;
}