# Frontend Composition Arrangement Handoff

Backend project schema `2` separates musical content from arrangement.

## Project Shape

```ts
type MeasureId = number;
type OutputId = string;

type MeasureBank = {
  next_id: MeasureId;
  measures: Array<{
    id: MeasureId;
    measure: { cell: Cell };
  }>;
};

type Composition = {
  columns: Array<{
    length: { numerator: number; denominator: number };
  }>;
  rows: Array<{
    output_id: OutputId;
    cells: Array<MeasureId | null>;
  }>;
};
```

Defaults imitate the previous app behavior:

- one measure in the bank with ID `1`;
- one composition column with length `4/4`;
- one composition row assigned to output ID `"current"`;
- the only composition cell references measure ID `1`.

## Semantics

- Measures contain only musical cell data. They no longer carry length or time
  signature.
- Column metadata owns musical length. Every row shares the same columns.
- Rows are tracks. `row.output_id` decides which plugin instance should render that
  row.
- `cells[column] === null` is an empty arrangement slot/rest.
- Multiple cells may reference the same measure ID. Editing a measure changes every
  arrangement cell that references it.
- Multiple rows may use the same output ID.

## Backend API Surface

The C++ domain API is in `include/xen/composition.hpp`:

- measure bank: `create_measure`, `remove_measure`, `duplicate_measure`,
  `find_measure`, `update_measure`, `all_measures`;
- composition rows: `insert_row`, `remove_row`, `move_row`, `assign_row_output`;
- composition columns: `insert_column`, `remove_column`, `move_column`,
  `set_column_length`;
- matrix cells: `assign_measure_reference`, `clear_measure_reference`,
  `measure_reference_at`.

Command coverage has not been expanded for frontend arrangement editing yet. Existing
single-measure commands still edit the default arranged measure and first column length.

## Rendering Contract

The MIDI renderer walks the composition left to right. For the current output ID it:

- skips rows assigned to other output IDs;
- resolves non-null measure IDs through the measure bank;
- renders each referenced measure using that column's length;
- sums all column lengths for the loop duration.

Until IPC instance IDs exist, the default/current backend output ID is `"current"`.
