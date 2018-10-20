using System.Collections;
using System.Collections.Generic;
using UnityEditor;
using UnityEngine;

[CustomEditor(typeof(BezierCurve))]
public class BezierCurveInspector : Editor {
    private BezierCurve curve;
    private Transform handleTransform;
    private Quaternion handleRotation;

    private void OnSceneGUI() {
        curve = target as BezierCurve;
        handleTransform = curve.transform;
        handleRotation = Tools.pivotRotation == PivotRotation.Local ? handleTransform.rotation : Quaternion.identity;

        Vector3 start = updatePosition(ref curve.start);
        Vector3 end = updatePosition(ref curve.end);
        Vector3 tangentStart = updatePosition(ref curve.tangentStart);
        Vector3 tangentEnd = updatePosition(ref curve.tangentEnd);
        Handles.DrawBezier(
            start, end, tangentStart, tangentEnd,
            Color.white, Texture2D.whiteTexture, 1f);
        
        Handles.DrawDottedLine(start, tangentStart, 1f);
        Handles.DrawDottedLine(end, tangentEnd, 1f);
    }
    
    // in the curve space
    private Vector3 updatePosition(ref Vector3 position) {
        Vector3 pos = handleTransform.TransformPoint(position);
        EditorGUI.BeginChangeCheck();
        pos = Handles.DoPositionHandle(pos, handleRotation);
        if (EditorGUI.EndChangeCheck()) {
            Undo.RecordObject(curve, "Move position");
            EditorUtility.SetDirty(curve);
            position = handleTransform.InverseTransformPoint(pos);
        }

        return pos;
    }
}
