/*
  ==============================================================================

   This file is part of the JUCE library - "Jules' Utility Class Extensions"
   Copyright 2004-10 by Raw Material Software Ltd.

  ------------------------------------------------------------------------------

   JUCE can be redistributed and/or modified under the terms of the GNU General
   Public License (Version 2), as published by the Free Software Foundation.
   A copy of the license is included in the JUCE distribution, or can be found
   online at www.gnu.org/licenses.

   JUCE is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
   A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

  ------------------------------------------------------------------------------

   To release a closed-source product which uses JUCE, commercial licenses are
   available: visit www.rawmaterialsoftware.com/juce for more information.

  ==============================================================================
*/

#include "../../../core/juce_StandardHeader.h"

BEGIN_JUCE_NAMESPACE

#include "juce_CallOutBox.h"
#include "juce_ComponentPeer.h"
#include "../lookandfeel/juce_LookAndFeel.h"
#include "../../../application/juce_Application.h"


//==============================================================================
CallOutBox::CallOutBox (Component& contentComponent,
                        Component& componentToPointTo,
                        Component* const parentComponent)
    : borderSpace (20), arrowSize (16.0f), content (contentComponent)
{
    addAndMakeVisible (&content);

    if (parentComponent != 0)
    {
        parentComponent->addChildComponent (this);

        updatePosition (parentComponent->getLocalArea (&componentToPointTo, componentToPointTo.getLocalBounds()),
                        parentComponent->getLocalBounds());

        setVisible (true);
    }
    else
    {
        if (! JUCEApplication::isStandaloneApp())
            setAlwaysOnTop (true); // for a plugin, make it always-on-top because the host windows are often top-level

        updatePosition (componentToPointTo.getScreenBounds(),
                        componentToPointTo.getParentMonitorArea());

        addToDesktop (ComponentPeer::windowIsTemporary);
    }
}

CallOutBox::~CallOutBox()
{
}

//==============================================================================
void CallOutBox::setArrowSize (const float newSize)
{
    arrowSize = newSize;
    borderSpace = jmax (20, (int) arrowSize);
    refreshPath();
}

void CallOutBox::paint (Graphics& g)
{
    if (background.isNull())
    {
        background = Image (Image::ARGB, getWidth(), getHeight(), true);
        Graphics g (background);
        getLookAndFeel().drawCallOutBoxBackground (*this, g, outline);
    }

    g.setColour (Colours::black);
    g.drawImageAt (background, 0, 0);
}

void CallOutBox::resized()
{
    content.setTopLeftPosition (borderSpace, borderSpace);
    refreshPath();
}

void CallOutBox::moved()
{
    refreshPath();
}

void CallOutBox::childBoundsChanged (Component*)
{
    updatePosition (targetArea, availableArea);
}

bool CallOutBox::hitTest (int x, int y)
{
    return outline.contains ((float) x, (float) y);
}

enum { callOutBoxDismissCommandId = 0x4f83a04b };

void CallOutBox::inputAttemptWhenModal()
{
    const Point<int> mousePos (getMouseXYRelative() + getBounds().getPosition());

    if (targetArea.contains (mousePos))
    {
        // if you click on the area that originally popped-up the callout, you expect it
        // to get rid of the box, but deleting the box here allows the click to pass through and
        // probably re-trigger it, so we need to dismiss the box asynchronously to consume the click..
        postCommandMessage (callOutBoxDismissCommandId);
    }
    else
    {
        exitModalState (0);
        setVisible (false);
    }
}

void CallOutBox::handleCommandMessage (int commandId)
{
    Component::handleCommandMessage (commandId);

    if (commandId == callOutBoxDismissCommandId)
    {
        exitModalState (0);
        setVisible (false);
    }
}

bool CallOutBox::keyPressed (const KeyPress& key)
{
    if (key.isKeyCode (KeyPress::escapeKey))
    {
        inputAttemptWhenModal();
        return true;
    }

    return false;
}

void CallOutBox::updatePosition (const Rectangle<int>& newAreaToPointTo, const Rectangle<int>& newAreaToFitIn)
{
    targetArea = newAreaToPointTo;
    availableArea = newAreaToFitIn;

    Rectangle<int> bounds (0, 0,
                           content.getWidth() + borderSpace * 2,
                           content.getHeight() + borderSpace * 2);

    const int hw = bounds.getWidth() / 2;
    const int hh = bounds.getHeight() / 2;
    const float hwReduced = (float) (hw - borderSpace * 3);
    const float hhReduced = (float) (hh - borderSpace * 3);
    const float arrowIndent = borderSpace - arrowSize;

    Point<float> targets[4] = { Point<float> ((float) targetArea.getCentreX(), (float) targetArea.getBottom()),
                                Point<float> ((float) targetArea.getRight(), (float) targetArea.getCentreY()),
                                Point<float> ((float) targetArea.getX(), (float) targetArea.getCentreY()),
                                Point<float> ((float) targetArea.getCentreX(), (float) targetArea.getY()) };

    Line<float> lines[4] = { Line<float> (targets[0].translated (-hwReduced, hh - arrowIndent), targets[0].translated (hwReduced, hh - arrowIndent)),
                             Line<float> (targets[1].translated (hw - arrowIndent, -hhReduced), targets[1].translated (hw - arrowIndent, hhReduced)),
                             Line<float> (targets[2].translated (-(hw - arrowIndent), -hhReduced), targets[2].translated (-(hw - arrowIndent), hhReduced)),
                             Line<float> (targets[3].translated (-hwReduced, -(hh - arrowIndent)), targets[3].translated (hwReduced, -(hh - arrowIndent))) };

    const Rectangle<float> centrePointArea (newAreaToFitIn.reduced (hw, hh).toFloat());

    float nearest = 1.0e9f;

    for (int i = 0; i < 4; ++i)
    {
        Line<float> constrainedLine (centrePointArea.getConstrainedPoint (lines[i].getStart()),
                                     centrePointArea.getConstrainedPoint (lines[i].getEnd()));

        const Point<float> centre (constrainedLine.findNearestPointTo (centrePointArea.getCentre()));
        float distanceFromCentre = centre.getDistanceFrom (centrePointArea.getCentre());

        if (! (centrePointArea.contains (lines[i].getStart()) || centrePointArea.contains (lines[i].getEnd())))
            distanceFromCentre *= 2.0f;

        if (distanceFromCentre < nearest)
        {
            nearest = distanceFromCentre;

            targetPoint = targets[i];
            bounds.setPosition ((int) (centre.getX() - hw),
                                (int) (centre.getY() - hh));
        }
    }

    setBounds (bounds);
}

void CallOutBox::refreshPath()
{
    repaint();
    background = Image::null;
    outline.clear();

    const float gap = 4.5f;
    const float cornerSize = 9.0f;
    const float cornerSize2 = 2.0f * cornerSize;
    const float arrowBaseWidth = arrowSize * 0.7f;
    const float left = content.getX() - gap, top = content.getY() - gap, right = content.getRight() + gap, bottom = content.getBottom() + gap;
    const float targetX = targetPoint.getX() - getX(), targetY = targetPoint.getY() - getY();

    outline.startNewSubPath (left + cornerSize, top);

    // if (targetY <= top)
    // {
    //     outline.lineTo (targetX - arrowBaseWidth, top);
    //     outline.lineTo (targetX, targetY);
    //     outline.lineTo (targetX + arrowBaseWidth, top);
    // }

    outline.lineTo (right - cornerSize, top);
    outline.addArc (right - cornerSize2, top, cornerSize2, cornerSize2, 0, float_Pi * 0.5f);

    // if (targetX >= right)
    // {
    //     outline.lineTo (right, targetY - arrowBaseWidth);
    //     outline.lineTo (targetX, targetY);
    //     outline.lineTo (right, targetY + arrowBaseWidth);
    // }

    outline.lineTo (right, bottom - cornerSize);
    outline.addArc (right - cornerSize2, bottom - cornerSize2, cornerSize2, cornerSize2, float_Pi * 0.5f, float_Pi);

    // if (targetY >= bottom)
    // {
    //     outline.lineTo (targetX + arrowBaseWidth, bottom);
    //     outline.lineTo (targetX, targetY);
    //     outline.lineTo (targetX - arrowBaseWidth, bottom);
    // }

    outline.lineTo (left + cornerSize, bottom);
    outline.addArc (left, bottom - cornerSize2, cornerSize2, cornerSize2, float_Pi, float_Pi * 1.5f);

    // if (targetX <= left)
    // {
    //     outline.lineTo (left, targetY + arrowBaseWidth);
    //     outline.lineTo (targetX, targetY);
    //     outline.lineTo (left, targetY - arrowBaseWidth);
    // }

    outline.lineTo (left, top + cornerSize);
    outline.addArc (left, top, cornerSize2, cornerSize2, float_Pi * 1.5f, float_Pi * 2.0f - 0.05f);

    outline.closeSubPath();
}

END_JUCE_NAMESPACE
