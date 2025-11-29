#ifndef DEEPSEEK_CLIENT_H
#define DEEPSEEK_CLIENT_H

/**
 * @brief Send text to DeepSeek API and get response
 * 
 * @param input_text User input text
 * @return char* AI response text (caller must free) or NULL on failure
 */
char* deepseek_chat(const char* input_text);

#endif // DEEPSEEK_CLIENT_H
