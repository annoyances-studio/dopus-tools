' Directory Opus RENAME script (OnGetNewName). Set the Rename dialog's script type to VBScript.
' Sorts photos/videos into YYYY-MM-DD\ folders.
' Date priority: metadata (date taken / release date) -> date embedded in the filename (YYYYMMDD_)
'                -> the OLDER of file created/modified -> today (last resort).
Option Explicit

Function OnGetNewName ( ByRef GetNewNameData )
	Dim item, meta
	Dim itemname
	Dim datestring
	Dim evaldate
	Dim dd, mm, yy

	Set item = GetNewNameData.item
	Set meta = item.metadata
	itemname = item.name

	' Strip common camera/phone prefixes so the embedded-date parse below can see the digits.
	If Mid(itemname, 1, 4) = "IMG_" Then
		itemname = Replace(itemname, "IMG_", "")
	ElseIf Mid(itemname, 1, 4) = "PXL_" Then
		itemname = Replace(itemname, "PXL_", "")
	End If

	If meta = "image" Then
		evaldate = meta.image.datetaken
	ElseIf meta = "video" Then
		evaldate = meta.video.releasedate
	End If

	If evaldate <> "" Then
		' 1) metadata date
		dd = Right("00" & Day(evaldate), 2)
		mm = Right("00" & Month(evaldate), 2)
		yy = Year(evaldate)
	ElseIf Mid(itemname, 9, 1) = "_" And IsNumeric(Mid(itemname, 1, 8)) Then
		' 2) date embedded in the name as YYYYMMDD_
		dd = Mid(itemname, 7, 2)
		mm = Mid(itemname, 5, 2)
		yy = Mid(itemname, 1, 4)
	Else
		' 3) the OLDER of created / modified.
		'    item.create/item.modify are DOpus Date objects; Day()/Month()/Year() coerce them to real
		'    VBScript dates (their .Format uses non-obvious codes). Compare via a numeric yyyymmdd key.
		Dim keyC, keyM
		keyC = Year(item.create) * 10000 + Month(item.create) * 100 + Day(item.create)
		keyM = Year(item.modify) * 10000 + Month(item.modify) * 100 + Day(item.modify)
		If keyM < keyC Then
			yy = Year(item.modify)
			mm = Right("00" & Month(item.modify), 2)
			dd = Right("00" & Day(item.modify), 2)
		Else
			yy = Year(item.create)
			mm = Right("00" & Month(item.create), 2)
			dd = Right("00" & Day(item.create), 2)
		End If
		' 4) last resort: recovered/zeroed files can carry a bogus 1601/1970 date -> use today.
		If yy < 1990 Then
			evaldate = Now()
			dd = Right("00" & Day(evaldate), 2)
			mm = Right("00" & Month(evaldate), 2)
			yy = Year(evaldate)
		End If
	End If

	datestring = yy & "-" & mm & "-" & dd
	OnGetNewName = datestring & "\" & item.name
End Function
