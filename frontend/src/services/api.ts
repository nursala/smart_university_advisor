export class ApiError extends Error {
  status: number

  constructor(message: string, status: number) {
    super(message)
    this.name = 'ApiError'
    this.status = status
  }
}

export function isAuthError(error: unknown): error is ApiError {
  return error instanceof ApiError && error.status === 401
}

type RequestOptions = {
  method?: 'GET' | 'POST' | 'PATCH' | 'DELETE'
  body?: unknown
  token?: string | null
  suppressUnauthorizedHandler?: boolean
}

const fallbackErrorMessage = 'Something went wrong. Please try again.'

const API_BASE_URL = import.meta.env.VITE_API_BASE_URL ?? '/api'
let authToken: string | null = null
let unauthorizedHandler: (() => void) | null = null

export function configureApiAuth(
  token: string | null,
  onUnauthorized: (() => void) | null,
) {
  authToken = token
  unauthorizedHandler = onUnauthorized
}

export async function apiRequest<T>(
  path: string,
  options: RequestOptions = {},
): Promise<T> {
  const headers: Record<string, string> = {}
  if (options.body !== undefined) {
    headers['Content-Type'] = 'application/json'
  }
  const requestToken = options.token === undefined ? authToken : options.token
  if (requestToken) {
    headers.Authorization = `Bearer ${requestToken}`
  }

  let response: Response
  try {
    response = await fetch(`${API_BASE_URL}${path}`, {
      method: options.method ?? 'GET',
      headers,
      body: options.body !== undefined ? JSON.stringify(options.body) : undefined,
    })
  } catch {
    throw new ApiError('Unable to reach the server. Please try again.', 0)
  }

  const data = await readJson(response)

  if (!response.ok) {
    const message =
      typeof (data as { error?: unknown }).error === 'string'
        ? (data as { error: string }).error
        : fallbackErrorMessage
    if ((response.status === 401 || response.status === 403) &&
        !options.suppressUnauthorizedHandler) unauthorizedHandler?.()
    throw new ApiError(message, response.status)
  }

  return data as T
}

async function readJson(response: Response): Promise<unknown> {
  try {
    return await response.json()
  } catch {
    return {}
  }
}
