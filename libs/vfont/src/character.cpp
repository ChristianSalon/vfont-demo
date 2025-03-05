/**
 * @file character.cpp
 * @author Christian Saloň
 */

#include "character.h"

namespace vft {

/**
 * @brief Character constructor
 *
 * @param glyph Glyph of character
 * @param codePoint Unicode code point
 * @param font Font of character
 * @param fontSize Font size of character
 * @param color Color of character
 * @param position Position of character relative to text block
 * @param transform Text block model matrix
 */
Character::Character(uint32_t glyphId, uint32_t codePoint, std::shared_ptr<Font> font, unsigned int fontSize)
    : _glyphId{glyphId}, _codePoint{codePoint}, _font{font}, _fontSize{fontSize} {
    this->_updateModelMatrix();
}

void Character::setAdvance(glm::vec2 advance) {
    this->_advance = advance;
}

void Character::setPosition(glm::vec2 position) {
    this->_position = position;
    this->_updateModelMatrix();
}

void Character::setTransform(glm::mat4 transform) {
    this->_parentMatrix = transform;
    this->_updateModelMatrix();
}

uint32_t Character::getGlyphId() const {
    return this->_glyphId;
}

/**
 * @brief Getter for unicode code point
 *
 * @return Unicode code point of character
 */
uint32_t Character::getCodePoint() const {
    return this->_codePoint;
}

glm::vec2 Character::getAdvance() const {
    return this->_advance;
}

/**
 * @brief Getter for character position relative to text block
 *
 * @return (X, Y) coordinates of character
 */
glm::vec2 Character::getPosition() const {
    return this->_position;
}

/**
 * @brief Getter for character model matrix
 *
 * @return Model matrix
 */
glm::mat4 Character::getModelMatrix() const {
    return this->_modelMatrix;
}

std::shared_ptr<Font> Character::getFont() const {
    return this->_font;
}

unsigned int Character::getFontSize() const {
    return this->_fontSize;
}

/**
 * @brief Updates the model matrix of character
 */
void Character::_updateModelMatrix() {
    glm::vec2 scale = this->_font->getScalingVector(this->_fontSize);
    this->_modelMatrix = this->_parentMatrix * glm::translate(glm::mat4(1.f), glm::vec3(this->_position, 0.f)) *
                         glm::rotate(glm::mat4(1.f), glm::radians(180.f), glm::vec3(1.f, 0.f, 0.f)) *
                         glm::scale(glm::mat4(1.f), glm::vec3(scale.x, scale.y, 0.f));
}

}  // namespace vft
