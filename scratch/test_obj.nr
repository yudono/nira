create_department(name):
  return {
    name: name,
    sub_departments: []
  }

add_sub_dept(dept, sub):
  subs = dept.sub_departments
  subs.push(sub)

main():
  head_office = create_department("Head Office")
  tech = create_department("Technology")
  
  print("Before:")
  print(head_office.sub_departments.length)
  
  add_sub_dept(head_office, tech)
  
  print("After:")
  print(head_office.sub_departments.length)
  
  for sub in head_office.sub_departments:
    print(sub.name)
