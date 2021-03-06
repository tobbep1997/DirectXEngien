#include "Window.h"
#include "Extern.h"

ID3D11Device*			DX::g_device;
ID3D11DeviceContext*	DX::g_deviceContext;

std::vector	<Drawable*>	DX::geometry;
std::vector	<Light*>	DX::lights;

DirectX::XMFLOAT4X4A	DX::shadowViewProjection;

void DX::safeRelease(IUnknown * u)
{
	if (u)
		u->Release();
	u = NULL;
}


bool Window::_initWindow()
{
	HICON hIicon = (HICON)LoadImage( // returns a HANDLE so we have to cast to HICON
		NULL,				// hInstance must be NULL when loading from a file
		NULL,				// the icon file name
		IMAGE_ICON,			// specifies that the file is an icon
		0,					// width of the image (we'll specify default later on)
		0,					// height of the image
		LR_LOADFROMFILE |	// we want to load a file (as opposed to a resource)
		LR_DEFAULTSIZE |	// default metrics based on the type (IMAGE_ICON, 32x32)
		LR_SHARED			// let the system release the handle when it's no longer used
	);

	WNDCLASSEX wcex = { 0 };
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = StaticWndProc;
	wcex.hInstance = m_hInstance;
	wcex.lpszClassName = "Engien";
	wcex.hIcon = hIicon;
	if (!RegisterClassEx(&wcex))
	{
		m_hwnd = false;
		return false;
	}

	RECT rc = { 0, 0, static_cast<long>(m_width), static_cast<long>(m_height) };
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

	HWND handle = CreateWindow(
		wcex.lpszClassName,
		m_title,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		rc.right - rc.left,
		rc.bottom - rc.top,
		nullptr,
		nullptr,
		m_hInstance,
		this);

	m_hwnd = handle;
	return true;
}

HRESULT Window::_initDirect3DContext()
{
	UINT createDeviceFlags = 0;

#ifdef _DEBUG
	createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif //DEBUG

	DXGI_SWAP_CHAIN_DESC scd;
	ZeroMemory(&scd, sizeof(DXGI_SWAP_CHAIN_DESC));

	// fill the swap chain description struct
	scd.BufferCount = 1;                                    // one back buffer
	scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;     // use 32-bit color
	scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;      // how swap chain is to be used
	scd.OutputWindow = m_hwnd;								// the window to be used
	scd.SampleDesc.Count = m_sampleCount;                   // how many multisamples
	scd.Windowed = !m_fullscreen;							// windowed/full-screen mode

															// create a device, device context and swap chain using the information in the scd struct
	HRESULT hr = D3D11CreateDeviceAndSwapChain(NULL,
		D3D_DRIVER_TYPE_HARDWARE,
		NULL,
		D3D11_CREATE_DEVICE_DEBUG,//D3D11_CREATE_DEVICE_DEBUG,
		NULL,
		NULL,
		D3D11_SDK_VERSION,
		&scd,
		&m_swapChain,
		&DX::g_device,
		NULL,
		&DX::g_deviceContext);

	if (SUCCEEDED(hr))
	{
		// get the address of the back buffer
		ID3D11Texture2D* pBackBuffer = nullptr;
		m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
		// use the back buffer address to create the render target
		DX::g_device->CreateRenderTargetView(pBackBuffer, NULL, &m_backBufferRTV);
		//we are creating the standard depth buffer here.
		_createDepthBuffer();

		DX::g_deviceContext->OMSetRenderTargets(1, &m_backBufferRTV, m_depthStencilView);	//As a standard we set the rendertarget. But it will be changed in the prepareGeoPass
		pBackBuffer->Release();
	}
	return hr;
}

void Window::_createDepthBuffer()
{
	D3D11_TEXTURE2D_DESC depthStencilDesc;

	depthStencilDesc.Width = m_width;
	depthStencilDesc.Height = m_height;
	depthStencilDesc.MipLevels = 1;
	depthStencilDesc.ArraySize = 1;
	depthStencilDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthStencilDesc.SampleDesc.Count = m_sampleCount;
	depthStencilDesc.SampleDesc.Quality = 0;
	depthStencilDesc.Usage = D3D11_USAGE_DEFAULT;
	depthStencilDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	depthStencilDesc.CPUAccessFlags = 0;
	depthStencilDesc.MiscFlags = 0;

	//Create the Depth/Stencil View
	HRESULT hr = DX::g_device->CreateTexture2D(&depthStencilDesc, NULL, &m_depthBufferTex);
	hr = DX::g_device->CreateDepthStencilView(m_depthBufferTex, NULL, &m_depthStencilView);

}

void Window::_initViewPort()
{
	m_viewport.Width = static_cast<float>(m_width);
	m_viewport.Height = static_cast<float>(m_height);
	m_viewport.MinDepth = 0.0f;
	m_viewport.MaxDepth = 1.0f;
	m_viewport.TopLeftX = 0;
	m_viewport.TopLeftY = 0;
}

void Window::_setViewport()
{
	DX::g_deviceContext->RSSetViewports(1, &m_viewport);
}

void Window::_createBuffers()
{
	D3D11_BUFFER_DESC vertexConstant;
	vertexConstant.Usage = D3D11_USAGE_DYNAMIC;
	vertexConstant.ByteWidth = sizeof(VERTEX_BUFFER);
	vertexConstant.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	vertexConstant.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	vertexConstant.MiscFlags = 0;
	vertexConstant.StructureByteStride = 0;

	HRESULT hr = DX::g_device->CreateBuffer(&vertexConstant, nullptr, &m_constantBuffer);

	D3D11_BUFFER_DESC lightConstant;
	lightConstant.Usage = D3D11_USAGE_DYNAMIC;
	lightConstant.ByteWidth = sizeof(LIGHT_BUFFER);
	lightConstant.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	lightConstant.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	lightConstant.MiscFlags = 0;
	lightConstant.StructureByteStride = 0;

	hr = DX::g_device->CreateBuffer(&lightConstant, nullptr, &this->m_lightBuffer);

	D3D11_BUFFER_DESC cameraBuffer;
	cameraBuffer.Usage = D3D11_USAGE_DYNAMIC;
	cameraBuffer.ByteWidth = sizeof(CAMERA_BUFFER);
	cameraBuffer.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cameraBuffer.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	cameraBuffer.MiscFlags = 0;
	cameraBuffer.StructureByteStride = 0;

	hr = DX::g_device->CreateBuffer(&cameraBuffer, nullptr, &this->m_cameraBuffer);

	D3D11_BUFFER_DESC shadowBuffer;
	shadowBuffer.Usage = D3D11_USAGE_DYNAMIC;
	shadowBuffer.ByteWidth = sizeof(XMFLOAT4X4A);
	shadowBuffer.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	shadowBuffer.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	shadowBuffer.MiscFlags = 0;
	shadowBuffer.StructureByteStride = 0;

	hr = DX::g_device->CreateBuffer(&shadowBuffer, nullptr, &this->m_shadowBuffer);

	D3D11_BUFFER_DESC texInfo;
	texInfo.Usage = D3D11_USAGE_DYNAMIC;
	texInfo.ByteWidth = sizeof(TEX_INFO);
	texInfo.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	texInfo.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	texInfo.MiscFlags = 0;
	texInfo.StructureByteStride = 0;

	hr = DX::g_device->CreateBuffer(&texInfo, nullptr, &this->m_texInfoBuffer);

	
}

void Window::_createShaders()
{
	ID3DBlob * blob = nullptr;
	Creator::createVertexShader(L"Shader/VertexShader.hlsl", &m_vertexShader, &blob, false);

	D3D11_INPUT_ELEMENT_DESC inputDesc[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0 },

	};
	HRESULT hr = DX::g_device->CreateInputLayout(inputDesc, ARRAYSIZE(inputDesc), blob->GetBufferPointer(), blob->GetBufferSize(), &m_inputLayout);
	hr = Creator::createPixelShader(L"Shader/PixelShader.hlsl", &m_pixelShader, &blob);

	hr = Creator::createVertexShader(L"Shader/VertexShadow.hlsl", &m_vertexShadow, &blob, false);
}

void Window::_present(int sync)
{
	m_swapChain->Present(0, 0);
}

void Window::_mapBuffers(Camera * camera)
{
	D3D11_MAPPED_SUBRESOURCE dataPtr;

	CAMERA_BUFFER camBuff = CAMERA_BUFFER();
	camBuff.position = camera->GetPosition();
	DX::g_deviceContext->Map(m_cameraBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &dataPtr);
	memcpy(dataPtr.pData, &camBuff, sizeof(CAMERA_BUFFER));
	DX::g_deviceContext->Unmap(m_cameraBuffer, 0);

	DX::g_deviceContext->PSSetConstantBuffers(1, 1, &m_cameraBuffer);

	DX::g_deviceContext->Map(m_shadowBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &dataPtr);
	memcpy(dataPtr.pData, &DX::shadowViewProjection, sizeof(XMFLOAT4X4A));
	DX::g_deviceContext->Unmap(m_shadowBuffer, 0);

	DX::g_deviceContext->PSSetConstantBuffers(2, 1, &m_shadowBuffer);
}

void Window::_geometryPass(Camera * camera)
{
	DX::g_deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	DX::g_deviceContext->IASetInputLayout(m_inputLayout);
	DX::g_deviceContext->VSSetShader(m_vertexShader, nullptr, 0);
	DX::g_deviceContext->HSSetShader(nullptr, nullptr, 0);
	DX::g_deviceContext->DSSetShader(nullptr, nullptr, 0);
	DX::g_deviceContext->GSSetShader(nullptr, nullptr, 0);
	DX::g_deviceContext->PSSetShader(nullptr, nullptr, 0);
	DX::g_deviceContext->PSSetShader(m_pixelShader, nullptr, 0);
	DX::g_deviceContext->RSSetViewports(1, &m_viewport);
	DX::g_deviceContext->OMSetRenderTargets(1, &m_backBufferRTV, m_depthStencilView);
	
	

	UINT32 vertexSize = sizeof(VERTEX);
	UINT32 offset = 0;

	ID3D11Buffer * vertexBuffer;
	VERTEX_BUFFER V_Buffer = VERTEX_BUFFER();

	if (camera)
	{
		V_Buffer.viewProjection = camera->GetViewProjectionMatrix();
	}
	else {
		V_Buffer.viewProjection = DirectX::XMFLOAT4X4A();
	}
	
	DX::g_deviceContext->PSSetShaderResources(3, 1, &m_shadowShaderResourceView);
	DX::g_deviceContext->PSSetSamplers(1, 1, &m_shadowSamplerState);

	D3D11_MAPPED_SUBRESOURCE dataPtr;
	TEX_INFO texInfo;
	texInfo.pad2 = false;

	for (size_t i = 0; i < DX::geometry.size(); i++)
	{
		V_Buffer.worldMatrix = DX::geometry[i]->getWorldMatrix();

		for (size_t j = 0; j < DX::geometry[i]->getObjectSize(); j++)
		{
			texInfo.pad2 = FALSE;
			texInfo.specMap = FALSE;
			texInfo.texture = FALSE;
			texInfo.normal = FALSE;

			Drawable * d = DX::geometry[i];
			vertexBuffer = DX::geometry[i]->getVertexBuffer()[j];
			DX::g_deviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &vertexSize, &offset);


			DX::g_deviceContext->Map(m_constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &dataPtr);
			memcpy(dataPtr.pData, &V_Buffer, sizeof(VERTEX_BUFFER));
			DX::g_deviceContext->Unmap(m_constantBuffer, 0);
		
			DX::g_deviceContext->VSSetConstantBuffers(0, 1, &m_constantBuffer);
			if (i < d->GetMaterial().size())
			{
				ID3D11SamplerState * ss;
				ID3D11ShaderResourceView * srv;
				if (DX::geometry[i]->GetMaterial()[j]->GetTexture())
				{
					ss = DX::geometry[i]->GetMaterial()[j]->GetTexture()->GetSamplerState();
					srv = DX::geometry[i]->GetMaterial()[j]->GetTexture()->GetShaderResourceView();
					DX::g_deviceContext->PSSetSamplers(0, 1, &ss);
					DX::g_deviceContext->PSSetShaderResources(0, 1, &srv);

					texInfo.texture = TRUE;
				}
				if (DX::geometry[i]->GetMaterial()[j]->GetNormalMap())
				{
					srv = DX::geometry[i]->GetMaterial()[j]->GetNormalMap()->GetShaderResourceView();

					DX::g_deviceContext->PSSetShaderResources(1, 1, &srv);
					texInfo.normal = TRUE;
				}
				if (DX::geometry[i]->GetMaterial()[j]->GetSpecularHighlightMap())
				{
					srv = DX::geometry[i]->GetMaterial()[j]->GetSpecularHighlightMap()->GetShaderResourceView();

					DX::g_deviceContext->PSSetShaderResources(2, 1, &srv);
					texInfo.specMap = TRUE;
				}
			}
			else
			{
				ID3D11SamplerState * ss;
				ID3D11ShaderResourceView * srv;
				if (DX::geometry[i]->GetMaterial()[0]->GetTexture())
				{
					ss = DX::geometry[i]->GetMaterial()[0]->GetTexture()->GetSamplerState();
					srv = DX::geometry[i]->GetMaterial()[0]->GetTexture()->GetShaderResourceView();
					DX::g_deviceContext->PSSetSamplers(0, 1, &ss);
					DX::g_deviceContext->PSSetShaderResources(0, 1, &srv);

					texInfo.texture = TRUE;
				}
				if (DX::geometry[i]->GetMaterial()[0]->GetNormalMap())
				{
					srv = DX::geometry[i]->GetMaterial()[0]->GetNormalMap()->GetShaderResourceView();

					DX::g_deviceContext->PSSetShaderResources(1, 1, &srv);
					texInfo.normal = TRUE;

				}
				if (DX::geometry[i]->GetMaterial()[0]->GetSpecularHighlightMap())
				{
					srv = DX::geometry[i]->GetMaterial()[0]->GetSpecularHighlightMap()->GetShaderResourceView();

					DX::g_deviceContext->PSSetShaderResources(2, 1, &srv);
					texInfo.specMap = TRUE;
				}
			}


			DX::g_deviceContext->Map(m_texInfoBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &dataPtr);
			memcpy(dataPtr.pData, &texInfo, sizeof(TEX_INFO));
			DX::g_deviceContext->Unmap(m_texInfoBuffer, 0);

			DX::g_deviceContext->PSSetConstantBuffers(3, 1, &m_texInfoBuffer);

			DX::g_deviceContext->Draw(DX::geometry[i]->getVertexSize()[j], 0);

		}
	}
}

void Window::_lightPass()
{
	LIGHT_BUFFER light_buffer = LIGHT_BUFFER();
	for (size_t i = 0; i < DX::lights.size(); i++)
	{
		light_buffer.info[i] =		XMINT4(static_cast<int>(DX::lights.size()), DX::lights[i]->GetInfo(), DX::lights[i]->GetCastShadow(),0);
		light_buffer.position[i] =	DX::lights[i]->GetPosition();
		light_buffer.direction[i] = DX::lights[i]->GetDirection();
		light_buffer.color[i] =		DX::lights[i]->GetColor();
	}
	for (size_t i = DX::lights.size(); i < 64; i++)
	{
		light_buffer.info[i] =		XMINT4(0, 0, 0, 0);
		light_buffer.position[i] =	XMFLOAT4A(0, 0, 0, 0);
		light_buffer.direction[i] = XMFLOAT4A(0, 0, 0, 0);
		light_buffer.color[i] =		XMFLOAT4A(0, 0, 0, 0);
	}
	D3D11_MAPPED_SUBRESOURCE dataPtr;
	DX::g_deviceContext->Map(m_lightBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &dataPtr);
	memcpy(dataPtr.pData, &light_buffer, sizeof(LIGHT_BUFFER));
	DX::g_deviceContext->Unmap(m_lightBuffer, 0);

	DX::g_deviceContext->PSSetConstantBuffers(0, 1, &m_lightBuffer);
}

void Window::_loadShadow()
{
	HRESULT hr;

	D3D11_TEXTURE2D_DESC texDesc;
	texDesc.Width = 4096;
	texDesc.Height = 4096;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	texDesc.CPUAccessFlags = 0;
	texDesc.MiscFlags = 0;

	hr = DX::g_device->CreateTexture2D(&texDesc, NULL, &m_shadowDepthBufferTex);

	D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	dsvDesc.Flags = 0;
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Texture2D.MipSlice = 0;

	hr = DX::g_device->CreateDepthStencilView(m_shadowDepthBufferTex, &dsvDesc, &m_shadowDepthStencilView);

	m_shadowViewport.Width = 4096;
	m_shadowViewport.Height = 4096;
	m_shadowViewport.MinDepth = 0.0f;
	m_shadowViewport.MaxDepth = 1.0f;
	m_shadowViewport.TopLeftX = 0;
	m_shadowViewport.TopLeftY = 0;

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = texDesc.MipLevels;
	srvDesc.Texture2D.MostDetailedMip = 0;

	hr = DX::g_device->CreateShaderResourceView(m_shadowDepthBufferTex, &srvDesc, &m_shadowShaderResourceView);

	D3D11_SAMPLER_DESC samplerDescPoint;
	samplerDescPoint.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
	samplerDescPoint.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
	samplerDescPoint.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
	samplerDescPoint.BorderColor[0] = 1.0f;
	samplerDescPoint.BorderColor[1] = 1.0f;
	samplerDescPoint.BorderColor[2] = 1.0f;
	samplerDescPoint.BorderColor[3] = 1.0f;
	samplerDescPoint.MinLOD = 0.f;
	samplerDescPoint.MaxLOD = D3D11_FLOAT32_MAX;
	samplerDescPoint.MipLODBias = 0.f;
	samplerDescPoint.MaxAnisotropy = 0;
	samplerDescPoint.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
	samplerDescPoint.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;

	hr = DX::g_device->CreateSamplerState(&samplerDescPoint, &m_shadowSamplerState);

}

void Window::_shadowPass()
{
	DX::g_deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	DX::g_deviceContext->IASetInputLayout(m_inputLayout);
	DX::g_deviceContext->VSSetShader(m_vertexShadow, nullptr, 0);
	DX::g_deviceContext->HSSetShader(nullptr, nullptr, 0);
	DX::g_deviceContext->DSSetShader(nullptr, nullptr, 0);
	DX::g_deviceContext->GSSetShader(nullptr, nullptr, 0);
	DX::g_deviceContext->PSSetShader(nullptr, nullptr, 0);
	DX::g_deviceContext->RSSetViewports(1, &m_shadowViewport);
	DX::g_deviceContext->OMSetRenderTargets(0, nullptr, m_shadowDepthStencilView);
	

	UINT32 vertexSize = sizeof(VERTEX);
	UINT32 offset = 0;
	DX::g_deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	ID3D11Buffer * vertexBuffer;
	VERTEX_BUFFER V_Buffer = VERTEX_BUFFER();

	V_Buffer.viewProjection = DX::shadowViewProjection;

	D3D11_MAPPED_SUBRESOURCE dataPtr;
	
	for (size_t i = 0; i < DX::geometry.size(); i++)
	{
		V_Buffer.worldMatrix = DX::geometry[i]->getWorldMatrix();

		for (size_t j = 0; j < DX::geometry[i]->getObjectSize(); j++)
		{
			Drawable * d = DX::geometry[i];
			vertexBuffer = DX::geometry[i]->getVertexBuffer()[j];
			DX::g_deviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &vertexSize, &offset);


			DX::g_deviceContext->Map(m_constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &dataPtr);
			memcpy(dataPtr.pData, &V_Buffer, sizeof(VERTEX_BUFFER));
			DX::g_deviceContext->Unmap(m_constantBuffer, 0);

			DX::g_deviceContext->VSSetConstantBuffers(0, 1, &m_constantBuffer);



			DX::g_deviceContext->Draw(DX::geometry[i]->getVertexSize()[j], 0);

		}
	}
}

void Window::_releaseIUnknown()
{
	DX::safeRelease(m_swapChain);
	DX::safeRelease(m_backBufferRTV);
	DX::safeRelease(m_depthStencilView);
	DX::safeRelease(m_depthBufferTex);
	DX::safeRelease(m_samplerState);
	DX::safeRelease(m_inputLayout);

	DX::safeRelease(m_vertexShader);
	DX::safeRelease(m_pixelShader);

	DX::safeRelease(m_constantBuffer);
	DX::safeRelease(m_lightBuffer);
	DX::safeRelease(m_cameraBuffer);
	DX::safeRelease(m_texInfoBuffer);

	DX::safeRelease(m_vertexShadow);
	DX::safeRelease(m_shadowInputLayout);
	DX::safeRelease(m_shadowDepthStencilView);
	DX::safeRelease(m_shadowDepthBufferTex);
	DX::safeRelease(m_shadowSamplerState);
	DX::safeRelease(m_shadowShaderResourceView);
	DX::safeRelease(m_shadowBuffer);

	DX::safeRelease(DX::g_deviceContext);
	DX::safeRelease(DX::g_device);
}

LRESULT Window::StaticWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	Window* pParent;
	if (uMsg == WM_CREATE)
	{
		pParent = (Window*)((LPCREATESTRUCT)lParam)->lpCreateParams;
		SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)pParent);
	}
	else
	{
		pParent = (Window*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
		if (!pParent) return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}

	pParent->m_hwnd = hWnd;
	return pParent->WndProc(uMsg, wParam, lParam);
}

LRESULT Window::WndProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	case WM_SIZE:
		m_width = LOWORD(lParam);
		m_height = HIWORD(lParam);
		break;
		// ----- Keyboard Button -----
	case WM_KEYDOWN:

		// --------------------------------Subject for change!--------------------------------
		if (wParam == VK_ESCAPE)
			PostQuitMessage(0);
		Input::_setKey(static_cast<UINT>(wParam), true);
		break;

	case WM_KEYUP:
		Input::_setKey(static_cast<UINT>(wParam), false);
		break;

	case WM_LBUTTONDOWN:
		Input::_setMouseKey(0, true);
		break;
	case WM_LBUTTONUP:
		Input::_setMouseKey(0, false);
		break;

	case WM_MBUTTONDOWN:
		Input::_setMouseKey(1, true);
		break;
	case WM_MBUTTONUP:
		Input::_setMouseKey(1, false);
		break;

	case WM_RBUTTONDOWN:
		Input::_setMouseKey(2, true);
		break;
	case WM_RBUTTONUP:
		Input::_setMouseKey(2, false);
		break;

	case WM_MOUSEMOVE:
		Input::_setMousePos(DirectX::XMFLOAT2(LOWORD(lParam), HIWORD(lParam)));
		break;

	case WM_MOUSEWHEEL:
		Input::_setScrollDelta(GET_WHEEL_DELTA_WPARAM(wParam) / 120.0f);
		break;
	}

	return DefWindowProc(m_hwnd, uMsg, wParam, lParam);
}

Window::Window(HINSTANCE h)
{
	m_hInstance = h;
	m_hwnd = nullptr;
	m_width = 0;
	m_height = 0;
	m_title = "";
	m_sampleCount = 1;
	m_fullscreen = FALSE;
	DX::g_device = nullptr;
	DX::g_deviceContext = nullptr;
	m_swapChain = nullptr;
	m_backBufferRTV = nullptr;
	m_depthStencilView = nullptr;
	m_depthBufferTex = nullptr;
	m_samplerState = nullptr;	

}

Window::~Window()
{
	this->_releaseIUnknown();
}

bool Window::Init(int width, int height, LPCSTR title, BOOL fullscreen)
{
	m_width = width;
	m_height = height;
	m_title = title;
	m_fullscreen = fullscreen;

	_initWindow();
	HRESULT hr = _initDirect3DContext();
	_initViewPort();
	_setViewport();

	ShowWindow(m_hwnd, 10);
	this->_createBuffers();
	this->_createShaders();
	this->_loadShadow();
	return false;
}

void Window::PollEvents()
{
	if (PeekMessage(&m_msg, nullptr, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&m_msg);
		DispatchMessage(&m_msg);
	}
}

bool Window::isOpen()
{
	return WM_QUIT != m_msg.message;
}

void Window::Clear()
{
	float c[4] = { 1.0f,0.0f,1.0f,1.0f };

	DX::g_deviceContext->ClearState();
	DX::g_deviceContext->ClearRenderTargetView(m_backBufferRTV, c);
	DX::g_deviceContext->ClearDepthStencilView(m_depthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	DX::g_deviceContext->ClearDepthStencilView(m_shadowDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	DX::g_deviceContext->OMSetBlendState(nullptr, 0, 0xffffffff);



	DX::lights.clear();
	DX::geometry.clear();

}

void Window::Flush(Camera * camera)
{
	if (camera)
		this->_mapBuffers(camera);
	this->_shadowPass();
	this->_lightPass();
	this->_geometryPass(camera);
	this->_present();
}
